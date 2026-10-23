#include "host/godot/lfw_godot_console.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <godot_cpp/classes/viewport.hpp>

namespace lfw::host {

namespace {

constexpr int kFontPx = 15;
constexpr int kHintPx = 12;
constexpr float kPad = 10.0f;
constexpr float kLineH = 20.0f;
constexpr float kInputH = 30.0f;
constexpr float kHintH = 22.0f;
constexpr float kScrollW = 12.0f;
constexpr size_t kMaxLines = 2000;
constexpr size_t kMaxHistory = 100;

std::string trim_ascii(const std::string& in) {
  const size_t b = in.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return std::string();
  const size_t e = in.find_last_not_of(" \t\r\n");
  return in.substr(b, e - b + 1);
}

std::string lower_ascii_copy(const std::string& in) {
  std::string out = in;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

}

void GodotConsole::create(godot::Node2D* owner) {
  if (_owner != nullptr || owner == nullptr) return;
  _owner = owner;

  // 输入框 / 滚动条放在自己的 CanvasLayer 里：游戏画面给宿主节点设了 clip（黑边裁切），
  // 子节点会被一起裁掉，只有独立 canvas 才不会被切
  _layer = memnew(godot::CanvasLayer);
  _layer->set_name("lfw_console_layer");
  _layer->set_layer(100);
  owner->add_child(_layer);

  _edit = memnew(godot::LineEdit);
  _edit->set_name("lfw_console_input");
  _edit->set_mouse_filter(godot::Control::MOUSE_FILTER_STOP);
  _edit->add_theme_font_size_override("font_size", kFontPx);
  _edit->add_theme_color_override("font_color", godot::Color(0.88f, 0.96f, 1.0f, 1.0f));
  _edit->add_theme_color_override("caret_color", godot::Color(0.55f, 1.0f, 0.85f, 1.0f));
  _edit->add_theme_color_override("font_placeholder_color",
                                  godot::Color(0.55f, 0.66f, 0.72f, 0.85f));
  _edit->set_placeholder(
      godot::String::utf8("输入命令（Enter 执行 · ↑↓ 历史 · Ctrl+` 或 Esc 关闭）"));
  _edit->hide();
  _layer->add_child(_edit);

  // 日志区右边的滚动条：拖 / 点 / 滚轮都由 ScrollBar 自己处理
  _bar = memnew(godot::VScrollBar);
  _bar->set_name("lfw_console_scroll");
  _bar->set_min(0.0);
  _bar->set_step(1.0);
  _bar->set_page(1.0);
  _bar->hide();
  _layer->add_child(_bar);
}

void GodotConsole::request_redraw() {
  if (_owner != nullptr) _owner->queue_redraw();
}

int GodotConsole::max_scroll() const {
  const int rows = metrics(_window).rows;
  return std::max(0, static_cast<int>(_lines.size()) - rows);
}

GodotConsole::Metrics GodotConsole::metrics(const godot::Vector2i& window) const {
  Metrics m;
  const float win_h = static_cast<float>(window.y);
  // 面板铺满窗口：输入框永远钉在窗口最下面，日志从下往上排，能显示多少行就显示多少行
  m.panel_h = win_h;
  m.input_top = win_h - kPad - kInputH;
  const float log_bottom = m.input_top - 6.0f - kHintH;
  const float log_top = kPad + static_cast<float>(kFontPx);
  m.rows = std::max(1, static_cast<int>((log_bottom - log_top) / kLineH));
  return m;
}

void GodotConsole::open() {
  if (_open || _edit == nullptr) return;
  _open = true;
  _history_pos = -1;
  _scroll = 0;
  _draft.clear();
  // 首帧 `_process` 之前就可能要画：先用视口兜个底，别让面板画成 0×0
  if (_owner != nullptr) {
    if (const godot::Viewport* const vp = _owner->get_viewport()) {
      _final = vp->get_final_transform();
      if (_window == godot::Vector2i()) _window = godot::Vector2i(vp->get_visible_rect().size);
    }
  }
  if (_on_open) _on_open();
  _edit->set_text(godot::String());
  _edit->show();
  if (_bar != nullptr) _bar->show();
  _edit->grab_focus();
  request_redraw();
}

void GodotConsole::close() {
  if (!_open) return;
  _open = false;
  if (_edit != nullptr) {
    _edit->release_focus();
    _edit->hide();
    _edit->set_text(godot::String());
  }
  if (_bar != nullptr) _bar->hide();
  if (_on_close) _on_close();
  request_redraw();
}

void GodotConsole::toggle() {
  if (_open) {
    close();
  } else {
    open();
  }
}

void GodotConsole::sync(const godot::Vector2i& window, const godot::Transform2D& final_transform) {
  _window = window;
  _final = final_transform;
  if (_edit == nullptr) return;
  const godot::Transform2D inv = final_transform.affine_inverse();
  const godot::Vector2 p0 = inv.xform(godot::Vector2(0.0f, 0.0f));
  const godot::Vector2 p1 =
      inv.xform(godot::Vector2(static_cast<float>(window.x), static_cast<float>(window.y)));
  const float scale =
      (window.y > 0 && p1.y > p0.y) ? (p1.y - p0.y) / static_cast<float>(window.y) : 1.0f;
  const float input_h = kInputH * scale;
  const float pad = kPad * scale;
  // 底对齐：窗口怎么变，输入框都贴在最后一行
  _edit->set_position(godot::Vector2(p0.x + pad, p1.y - pad - input_h));
  _edit->set_size(godot::Vector2(std::max(48.0f, (p1.x - p0.x) - pad * 2.0f), input_h));
  sync_scrollbar(window, final_transform);
}

void GodotConsole::sync_scrollbar(const godot::Vector2i& window,
                                  const godot::Transform2D& final_transform) {
  if (_bar == nullptr) return;
  const Metrics m = metrics(window);
  const int total = static_cast<int>(_lines.size());
  const bool needed = total > m.rows;
  _bar->set_visible(needed);
  if (!needed) return;

  const godot::Transform2D inv = final_transform.affine_inverse();
  const godot::Vector2 p0 = inv.xform(godot::Vector2(0.0f, 0.0f));
  const godot::Vector2 p1 =
      inv.xform(godot::Vector2(static_cast<float>(window.x), static_cast<float>(window.y)));
  const float scale =
      (window.y > 0 && p1.y > p0.y) ? (p1.y - p0.y) / static_cast<float>(window.y) : 1.0f;
  const float bar_w = kScrollW * scale;
  const float top = p0.y + kPad * scale;
  const float bottom = p0.y + (m.input_top - kHintH) * scale;
  _bar->set_position(godot::Vector2(p1.x - bar_w - kPad * scale * 0.5f, top));
  _bar->set_size(godot::Vector2(bar_w, std::max(bar_w, bottom - top)));

  _bar->set_page(static_cast<double>(m.rows));
  _bar->set_max(static_cast<double>(total));
  const int scroll = std::clamp(_scroll, 0, std::max(0, total - m.rows));
  _bar->set_value_no_signal(static_cast<double>(total - m.rows - scroll));
}

void GodotConsole::scroll_by(int delta) {
  if (!_open || _lines.empty()) return;
  _scroll = std::clamp(_scroll + delta, 0, max_scroll());
  sync_scrollbar(_window, _final);
  request_redraw();
}

void GodotConsole::on_scrollbar_value(double value) {
  if (!_open) return;
  const int rows = metrics(_window).rows;
  const int total = static_cast<int>(_lines.size());
  // 滚动条的 value 是「顶部行号」，换算回「离底部多少行」
  const int top = static_cast<int>(std::lround(value));
  _scroll = std::clamp(total - rows - top, 0, std::max(0, total - rows));
  request_redraw();
}

void GodotConsole::append(int kind, const std::string& text) {
  size_t start = 0;
  int added = 0;
  while (start <= text.size()) {
    const size_t nl = text.find('\n', start);
    const std::string line =
        trim_ascii(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
    if (!line.empty()) {
      _lines.push_back(Line{line, kind});
      ++added;
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  if (_lines.size() > kMaxLines) {
    const size_t drop = _lines.size() - kMaxLines;
    _lines.erase(_lines.begin(), _lines.begin() + static_cast<std::ptrdiff_t>(drop));
  }
  // 已经上滚时保持视口不动（新行只会加在下面）；贴底时自然跟着最新行
  if (_scroll > 0) {
    _scroll += added;
    _scroll = std::min(_scroll, max_scroll());
  }
  if (_open) request_redraw();
}

void GodotConsole::execute(const std::string& raw) {
  const std::string line = trim_ascii(raw);
  if (line.empty()) return;
  const std::string lower = lower_ascii_copy(line);
  if (lower == "clear" || lower == "cls") {
    _lines.clear();
    _scroll = 0;
    request_redraw();
    return;
  }
  append(0, "> " + line);
  if (_history.empty() || _history.back() != line) {
    _history.push_back(line);
    if (_history.size() > kMaxHistory) _history.erase(_history.begin());
  }
  _history_pos = -1;
  _draft.clear();
  if (_exec) _exec(line);
}

void GodotConsole::submit(const std::string& line) {
  execute(line);
  if (_edit != nullptr) {
    _edit->set_text(godot::String());
    _edit->grab_focus();
  }
}

void GodotConsole::history_step(int delta) {
  if (_edit == nullptr || _history.empty()) return;
  if (_history_pos < 0 && delta > 0) return;
  if (_history_pos < 0) {
    _draft = _edit->get_text().utf8().get_data();
    _history_pos = static_cast<int>(_history.size()) - 1;
  } else if (delta < 0) {
    if (_history_pos > 0) --_history_pos;
  } else if (_history_pos + 1 < static_cast<int>(_history.size())) {
    ++_history_pos;
  } else {
    _history_pos = -1;
    _edit->set_text(godot::String::utf8(_draft.c_str()));
    _edit->set_caret_column(static_cast<int32_t>(_edit->get_text().length()));
    return;
  }
  _edit->set_text(godot::String::utf8(_history[static_cast<size_t>(_history_pos)].c_str()));
  _edit->set_caret_column(static_cast<int32_t>(_edit->get_text().length()));
}

bool GodotConsole::handle_key(const godot::Ref<godot::InputEventKey>& key) {
  if (key.is_null()) return false;
  godot::Key code = static_cast<godot::Key>(key->get_keycode());
  if (code == godot::KEY_NONE) code = static_cast<godot::Key>(key->get_physical_keycode());
  if (!key->is_pressed()) return _open;
  const bool quote = code == godot::KEY_QUOTELEFT;
  if (quote && (key->is_ctrl_pressed() || key->is_command_or_control_pressed())) {
    toggle();
    return true;
  }
  if (!_open) return false;
  if (code == godot::KEY_ESCAPE) {
    close();
    return true;
  }
  if (code == godot::KEY_UP) {
    history_step(-1);
    return true;
  }
  if (code == godot::KEY_DOWN) {
    history_step(1);
    return true;
  }
  if (code == godot::KEY_PAGEUP) {
    scroll_by(metrics(_window).rows);
    return true;
  }
  if (code == godot::KEY_PAGEDOWN) {
    scroll_by(-metrics(_window).rows);
    return true;
  }
  return false;  // 其余按键交给 LineEdit
}

bool GodotConsole::handle_mouse(const godot::Ref<godot::InputEventMouseButton>& mouse) {
  if (!_open || mouse.is_null() || !mouse->is_pressed()) return _open;
  const bool on_bar = _bar != nullptr && _bar->is_visible() &&
                      _bar->get_global_rect().has_point(mouse->get_position());
  if (on_bar) return false;  // 让 ScrollBar 自己吃
  const godot::MouseButton button = mouse->get_button_index();
  if (button == godot::MOUSE_BUTTON_WHEEL_UP) {
    scroll_by(3);
    return true;
  }
  if (button == godot::MOUSE_BUTTON_WHEEL_DOWN) {
    scroll_by(-3);
    return true;
  }
  // 点画面别的地方也不要把输入焦点丢掉，否则就再也打不进字了
  if (_edit != nullptr && !_edit->has_focus()) _edit->grab_focus();
  return false;
}

void GodotConsole::draw(godot::CanvasItem& canvas, const godot::Vector2i& window) {
  if (!_open) return;
  canvas.draw_set_transform_matrix(_final.affine_inverse());
  const float win_w = static_cast<float>(window.x);
  const Metrics m = metrics(window);
  canvas.draw_rect(godot::Rect2(godot::Vector2(0.0f, 0.0f), godot::Vector2(win_w, m.panel_h)),
                   godot::Color(0.02f, 0.03f, 0.05f, 0.90f), true);
  const float divider_y = m.input_top - kHintH;
  canvas.draw_rect(godot::Rect2(godot::Vector2(0.0f, divider_y), godot::Vector2(win_w, 1.0f)),
                   godot::Color(0.35f, 0.85f, 1.0f, 0.55f), true);

  godot::Ref<godot::Font> font;
  if (_font) font = _font("Arial");
  if (font.is_null()) {
    canvas.draw_set_transform_matrix(godot::Transform2D());
    return;
  }
  const int top_index_limit = std::max(0, static_cast<int>(_lines.size()) - m.rows);
  const int scroll = std::clamp(_scroll, 0, top_index_limit);
  const float hint_y = m.input_top - 6.0f;
  std::string hint = "控制台 · Enter 执行 · ↑↓ 历史 · PgUp/PgDn 翻页 · Ctrl+` / Esc 关闭";
  if (scroll > 0) {
    hint = "↑ 上滚 " + std::to_string(scroll) + " 行（PgDn / 滚轮回到底部）· " + hint;
  }
  canvas.draw_string(font, godot::Vector2(kPad, hint_y), godot::String::utf8(hint.c_str()),
                     godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, kHintPx,
                     godot::Color(0.52f, 0.62f, 0.70f, 0.95f));
  int drawn = 0;
  const int top = static_cast<int>(_lines.size()) - 1 - scroll;
  const float text_w = win_w - kPad * 2.0f - (top_index_limit > 0 ? kScrollW + 6.0f : 0.0f);
  for (int i = top; i >= 0 && drawn < m.rows; --i, ++drawn) {
    const Line& line = _lines[static_cast<size_t>(i)];
    const float y = divider_y - 6.0f - kLineH * static_cast<float>(drawn);
    if (y < static_cast<float>(kFontPx) + kPad) break;
    godot::Color color(0.80f, 0.86f, 0.92f, 1.0f);
    if (line.kind == 0) color = godot::Color(0.55f, 0.95f, 1.0f, 1.0f);
    else if (line.kind == 2) color = godot::Color(1.0f, 0.78f, 0.35f, 1.0f);
    else if (line.kind == 3) color = godot::Color(1.0f, 0.45f, 0.45f, 1.0f);
    canvas.draw_string(font, godot::Vector2(kPad, y), godot::String::utf8(line.text.c_str()),
                       godot::HORIZONTAL_ALIGNMENT_LEFT, text_w, kFontPx, color);
  }
  canvas.draw_set_transform_matrix(godot::Transform2D());
}

}
