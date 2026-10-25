#include "host/godot/lfw_godot_console.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/display_server.hpp>

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
      godot::String::utf8("输入命令（Enter 执行 · 上下键翻历史 · Ctrl+` 或 Esc 关闭）"));
  // 回车提交后 LineEdit 缺省会自己 `release_focus()`：控制台是连续输入的场景，
  // 清空输入后必须还停在输入框上，否则下一次敲键就"打不进去"了。
  _edit->set_keep_editing_on_text_submit(true);
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
  // 上下键是控制台自己吃掉的：焦点必须在输入框上，否则字写进去了却看不见光标、也打不了字
  if (!_edit->has_focus()) _edit->grab_focus();
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
  // 下面这些键都是控制台自己吃掉的（不经过 LineEdit）；焦点要在输入框上，
  // 否则翻历史把字写进去了却看不到光标、接着敲键也进不来。
  if (_edit != nullptr && !_edit->has_focus()) _edit->grab_focus();
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
  const bool ctrl = key->is_ctrl_pressed() || key->is_command_or_control_pressed();
  if (ctrl && code == godot::KEY_A) {
    select_all();
    return true;
  }
  if (ctrl && (code == godot::KEY_C || code == godot::KEY_INSERT)) {
    // 有选中就复制日志；没选中就交给 LineEdit 自己复制输入框里的字
    if (has_selection()) {
      _copied_hint = copy_selection();
      request_redraw();
      return true;
    }
    return false;
  }
  return false;  // 其余按键交给 LineEdit
}

bool GodotConsole::handle_mouse(const godot::Ref<godot::InputEventMouseButton>& mouse) {
  if (!_open || mouse.is_null()) return false;
  // 只有「滚轮翻页」是自己吃掉的；其余鼠标事件一律放行给 GUI（输入框点击、滚动条拖动
  // 都要靠 ScrollBar / LineEdit 自己的 `_gui_input`，这里标记 handled 会把它们的松开也吞掉）
  const bool on_bar = _bar != nullptr && _bar->is_visible() &&
                      _bar->get_global_rect().has_point(mouse->get_position());
  if (on_bar) {
    _dragging = false;
    return false;
  }
  const godot::MouseButton button = mouse->get_button_index();
  if (mouse->is_pressed()) {
    if (button == godot::MOUSE_BUTTON_WHEEL_UP) {
      scroll_by(3);
      return true;
    }
    if (button == godot::MOUSE_BUTTON_WHEEL_DOWN) {
      scroll_by(-3);
      return true;
    }
  }
  if (button == godot::MOUSE_BUTTON_LEFT) {
    int line = -1;
    int col = 0;
    if (mouse->is_pressed() && pos_at_window_pos(mouse->get_position(), line, col)) {
      if (mouse->is_double_click()) {
        // 双击 = 整行
        _dragging = false;
        _sel_a = SelPos{line, 0};
        _sel_b = SelPos{line, static_cast<int>(_lines[static_cast<size_t>(line)].text.size())};
      } else {
        _dragging = true;
        _sel_a = SelPos{line, col};
        _sel_b = _sel_a;
      }
      request_redraw();
    } else if (mouse->is_pressed()) {
      _dragging = false;
      _sel_a = SelPos{};
      _sel_b = SelPos{};
      request_redraw();
    } else {
      _dragging = false;
    }
  }
  // 点画面别的地方也不要把输入焦点丢掉，否则就再也打不进字了
  if (_edit != nullptr && !_edit->has_focus()) _edit->grab_focus();
  return false;
}

bool GodotConsole::handle_motion(const godot::Ref<godot::InputEventMouseMotion>& motion) {
  if (!_open || motion.is_null() || !_dragging) return false;
  int line = -1;
  int col = 0;
  if (pos_at_window_pos(motion->get_position(), line, col) &&
      (line != _sel_b.line || col != _sel_b.col)) {
    _sel_b = SelPos{line, col};
    request_redraw();
  }
  return false;
}

int GodotConsole::line_at_window_pos(const godot::Vector2& pos) const {
  int line = -1;
  int col = 0;
  return pos_at_window_pos(pos, line, col) ? line : -1;
}

bool GodotConsole::pos_at_window_pos(const godot::Vector2& pos, int& line, int& col) const {
  if (_vis_top < 0 || _vis_rows <= 0 || _mono.is_null()) return false;
  // 绘制时用的是 `_final.affine_inverse()`，所以窗口像素要先过一遍 `_final` 才落回同一坐标系
  const godot::Vector2 p = _final.xform(pos);
  if (p.x < kPad * 0.5f) return false;
  const int row = static_cast<int>(std::lround((_vis_text_top - p.y) / kLineH));
  if (row < 0 || row >= _vis_rows) return false;
  const int index = _vis_top - row;
  if (index < 0 || index >= static_cast<int>(_lines.size())) return false;
  line = index;
  col = col_at_x(_lines[static_cast<size_t>(index)].text, p.x - kPad);
  return true;
}

float GodotConsole::text_width(const std::string& text, int col) const {
  if (_mono.is_null()) return 0.0f;
  const int n = std::min(col, static_cast<int>(text.size()));
  if (n <= 0) return 0.0f;
  const godot::String s = godot::String::utf8(text.substr(0, static_cast<size_t>(n)).c_str());
  return _mono->get_string_size(s, godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, kFontPx).x;
}

int GodotConsole::col_at_x(const std::string& text, float x) const {
  if (_mono.is_null() || x <= 0.0f) return 0;
  int best = 0;
  int i = 0;
  const int size = static_cast<int>(text.size());
  while (i < size) {
    // 一个 UTF-8 码点的结束位置（跳过 10xxxxxx 续字节）
    int end = i + 1;
    while (end < size && (static_cast<unsigned char>(text[static_cast<size_t>(end)]) & 0xC0) ==
                             0x80) {
      ++end;
    }
    const float w = text_width(text, end);
    if (x < w) {
      const float prev = text_width(text, i);
      // 落在字符右半边 ⇒ 认为已经到下一个字符边界（和大多数终端的手感一致）
      return (x - prev < w - x) ? i : end;
    }
    best = end;
    i = end;
  }
  return best;
}

void GodotConsole::norm_selection(int& a_line, int& a_col, int& b_line, int& b_col) const {
  const bool swap = a_line > b_line || (a_line == b_line && a_col > b_col);
  if (swap) {
    std::swap(a_line, b_line);
    std::swap(a_col, b_col);
  }
}

void GodotConsole::select_all() {
  if (_lines.empty()) return;
  _sel_a = SelPos{0, 0};
  _sel_b = SelPos{static_cast<int>(_lines.size()) - 1,
                  static_cast<int>(_lines.back().text.size())};
  request_redraw();
}

bool GodotConsole::line_selected(int index) const {
  if (!has_selection()) return false;
  int a_line = _sel_a.line;
  int a_col = _sel_a.col;
  int b_line = _sel_b.line;
  int b_col = _sel_b.col;
  norm_selection(a_line, a_col, b_line, b_col);
  return index >= a_line && index <= b_line;
}

int GodotConsole::copy_selection() {
  if (!has_selection()) return 0;
  int a_line = _sel_a.line;
  int a_col = _sel_a.col;
  int b_line = _sel_b.line;
  int b_col = _sel_b.col;
  norm_selection(a_line, a_col, b_line, b_col);
  a_line = std::max(0, a_line);
  a_col = std::max(0, a_col);
  b_line = std::min(static_cast<int>(_lines.size()) - 1, b_line);
  std::string out;
  for (int i = a_line; i <= b_line; ++i) {
    const std::string& text = _lines[static_cast<size_t>(i)].text;
    const int from = std::min(i == a_line ? a_col : 0, static_cast<int>(text.size()));
    const int to = std::min(i == b_line ? b_col : static_cast<int>(text.size()),
                            static_cast<int>(text.size()));
    if (to <= from) continue;
    if (!out.empty()) out += '\n';
    out += text.substr(static_cast<size_t>(from), static_cast<size_t>(to - from));
  }
  if (out.empty()) return 0;
  if (godot::DisplayServer* const ds = godot::DisplayServer::get_singleton(); ds != nullptr) {
    ds->clipboard_set(godot::String::utf8(out.c_str()));
  }
  return b_line - a_line + 1;
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
  if (_font) font = _font("15px monospace");
  if (font.is_null()) font = _mono;
  if (font.is_null()) {
    canvas.draw_set_transform_matrix(godot::Transform2D());
    return;
  }
  _mono = font;
  const int top_index_limit = std::max(0, static_cast<int>(_lines.size()) - m.rows);
  const int scroll = std::clamp(_scroll, 0, top_index_limit);
  const float hint_y = m.input_top - 6.0f;
  std::string hint =
      "控制台 · Enter 执行 · 上下键翻历史 · PgUp/PgDn 翻页 · 拖选日志 + Ctrl+C 复制 · Ctrl+` / Esc 关闭";
  if (scroll > 0) {
    hint = "上滚 " + std::to_string(scroll) + " 行（PgDn / 滚轮回到底部）· " + hint;
  }
  if (_copied_hint > 0) {
    hint = "已复制 " + std::to_string(_copied_hint) + " 行到剪贴板 · " + hint;
  }
  canvas.draw_string(font, godot::Vector2(kPad, hint_y), godot::String::utf8(hint.c_str()),
                     godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, kHintPx,
                     godot::Color(0.52f, 0.62f, 0.70f, 0.95f));
  int drawn = 0;
  const int top = static_cast<int>(_lines.size()) - 1 - scroll;
  const float text_w = win_w - kPad * 2.0f - (top_index_limit > 0 ? kScrollW + 6.0f : 0.0f);
  const float ascent = font->get_ascent(kFontPx);
  for (int i = top; i >= 0 && drawn < m.rows; --i, ++drawn) {
    const Line& line = _lines[static_cast<size_t>(i)];
    const float y = divider_y - 6.0f - kLineH * static_cast<float>(drawn);
    if (y < static_cast<float>(kFontPx) + kPad) break;
    if (line_selected(i)) {
      int a_line = _sel_a.line;
      int a_col = _sel_a.col;
      int b_line = _sel_b.line;
      int b_col = _sel_b.col;
      norm_selection(a_line, a_col, b_line, b_col);
      const std::string& text = line.text;
      const float x0 = kPad + (i == a_line ? text_width(text, a_col) : 0.0f);
      const float x1 = kPad + (i == b_line ? text_width(text, b_col)
                                          : text_width(text, static_cast<int>(text.size())));
      if (x1 > x0) {
        canvas.draw_rect(
            godot::Rect2(godot::Vector2(x0 - 2.0f, y - ascent - 2.0f),
                         godot::Vector2(x1 - x0 + 4.0f, kLineH)),
            godot::Color(0.32f, 0.56f, 0.88f, 0.45f), true);
      }
    }
    godot::Color color(0.80f, 0.86f, 0.92f, 1.0f);
    if (line.kind == 0) color = godot::Color(0.55f, 0.95f, 1.0f, 1.0f);
    else if (line.kind == 2) color = godot::Color(1.0f, 0.78f, 0.35f, 1.0f);
    else if (line.kind == 3) color = godot::Color(1.0f, 0.45f, 0.45f, 1.0f);
    canvas.draw_string(font, godot::Vector2(kPad, y), godot::String::utf8(line.text.c_str()),
                       godot::HORIZONTAL_ALIGNMENT_LEFT, text_w, kFontPx, color);
  }
  // 记下可视区：鼠标拖选要按这份几何换行号
  _vis_top = top;
  _vis_rows = drawn;
  _vis_text_top = divider_y - 6.0f;
  _vis_text_w = text_w;
  canvas.draw_set_transform_matrix(godot::Transform2D());
}

}
