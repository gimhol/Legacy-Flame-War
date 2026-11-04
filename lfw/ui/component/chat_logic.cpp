#include "lfw/ui/component/chat_logic.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "lfw/core/js_num.h"
#include "lfw/core/value.h"
#include "lfw/lfw.h"
#include "lfw/net/chat_overlay.h"
#include "lfw/net/chat_store.h"
#include "lfw/ui/component/flex.h"
#include "lfw/ui/component/text_input.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

namespace {

constexpr double PEEK_HOLD_MS = 10000.0;
constexpr double PEEK_FADE_MS = 1500.0;
constexpr double ROW_W = 442.0;
constexpr double ROW_H = 18.0;
constexpr double ROW_GAP = 1.0;
constexpr double NAME_GAP = 4.0;
constexpr double LIST_LEFT = 10.0;
constexpr double LIST_BOTTOM_TYPING = 420.0;
constexpr double LIST_BOTTOM_PEEK = 446.0;
constexpr double LIST_MAX_H = 250.0;
/** 单条消息最多占几行（超出部分用省略号）。 */
constexpr int MAX_LINES = 3;
/** 名字最多占用的行宽比例，避免超长昵称把正文挤没。 */
constexpr double NAME_MAX_RATIO = 0.55;
constexpr double BG_W = 640.0;
/** 消息遮罩比消息列表多出的高度（收在输入框分割线之上）。 */
constexpr double BG_EXTRA_H = 4.0;

const char16_t* const ELLIPSIS = u"\u2026";
/** 不能出现在行首的字符（中文收尾标点、英文标点等）。 */
const char16_t* const NO_LINE_START =
    u"\uff0c\u3002\u3001\uff01\uff1f\uff1b\uff1a\uff09\u3011\u300d\u300f\u300b\u3009\u3015"
    u"\uff5d\u201d\u2019\u2026\u2025\u00b7,.!?;:)]}>%";
/** 不能出现在行尾的字符（中文开头标点）。 */
const char16_t* const NO_LINE_END =
    u"\uff08\u3010\u300c\u300e\u300a\u3008\u3014\uff5b\u201c\u2018([{<";

const char16_t* const BODY_COLOR = u"#f4f4f4";
const char16_t* const SYSTEM_COLOR = u"#ffd76e";

const char16_t* channel_color(net::ChatTarget target) {
  return target == net::ChatTarget::kRoom ? u"#7fc4ff" : u"#ffb44d";
}

bool contains(const char16_t* chars, char16_t c) {
  for (const char16_t* p = chars; *p != 0; ++p) {
    if (*p == c) return true;
  }
  return false;
}

/** 按码点切分（对齐 TS 的 `[...text]`，不会把代理对拆开）。 */
std::vector<std::u16string> code_points(const std::u16string& text) {
  std::vector<std::u16string> out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    const char16_t c = text[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 &&
        text[i + 1] <= 0xDFFF) {
      out.push_back(text.substr(i, 2));
      ++i;
    } else {
      out.push_back(text.substr(i, 1));
    }
  }
  return out;
}

std::u16string join(const std::vector<std::u16string>& parts, size_t from, size_t to) {
  std::u16string out;
  for (size_t i = from; i < to && i < parts.size(); ++i) out += parts[i];
  return out;
}

std::u16string trim_end(const std::u16string& text) {
  size_t end = text.size();
  while (end > 0 && (text[end - 1] == u' ' || text[end - 1] == u'\t')) --end;
  return text.substr(0, end);
}

char16_t char_at(const std::vector<std::u16string>& chars, size_t i) {
  return i < chars.size() && !chars[i].empty() ? chars[i][0] : 0;
}

void set_fill(UINode* node, const char16_t* color) {
  if (node == nullptr) return;
  const Value want(color);
  if (!strict_equals(node->style.fill_style(), want)) node->style.set_fill_style(want);
}

}  // namespace

const ClazzTag* ChatLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& ChatLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"ChatLogic"};
  return tags;
}

ChatLogic::ChatLogic(UINode& layout, const std::u16string& f_name, const Value& info)
    : UIComponent(layout, f_name, info) {}

ChatLogic::~ChatLogic() = default;

net::ChatStore& ChatLogic::store() { return lfw().chat_store(); }

UINode* ChatLogic::q(const char16_t* id) { return node.search_node(std::u16string(id)); }

void ChatLogic::on_start() {
  _closing = false;
  _locked = !store().peek;
  _list = q(u"chat_rows");
  _rows.clear();
  if (_list != nullptr) {
    for (UINode* const row : _list->children()) {
      if (row == nullptr || row->children().size() < 2) continue;
      _rows.push_back(Row{row->children()[0], row->children()[1]});
    }
  }
  _input_node = q(u"chat_input");
  _input = nullptr;
  if (_input_node != nullptr) {
    UIComponent* const comp = _input_node->find_component(TextInput::class_tag());
    if (comp != nullptr && is_class(comp->clazz(), TextInput::class_tag())) {
      _input = static_cast<TextInput*>(comp);
    }
  }
  if (_input != nullptr) {
    _input->on_submit = [this]() { send(); };
    _input->on_cancel = [this]() { exit_input(); };
  }

  if (UINode* const btn = q(u"chat_close")) {
    btn->callbacks.on_click = [this](LFWPointerEvent&) { close(); };
  }
  if (UINode* const btn = q(u"chat_send")) {
    btn->callbacks.on_click = [this](LFWPointerEvent&) { send(); };
  }
  if (UINode* const bg = q(u"chat_input_bg")) {
    bg->callbacks.on_click = [this](LFWPointerEvent&) { focus_input(); };
  }

  _hold = store().peek ? PEEK_HOLD_MS : 0.0;
  set_input_visible(!store().peek);
  // 预览态是纯展示浮层：整页不参与指针命中，否则会吞掉下层房间页的「准备」等按钮点击。
  node.set_touchable(!store().peek);
  _seen = static_cast<int>(store().items().size());
  render();
  _focus_pending = !store().peek;
  _focus_frames = 0;
}

void ChatLogic::update(double dt) {
  if (_closing) return;
  if (_locked) store().peek = false;
  if (_focus_pending) {
    const bool focused = _input != nullptr && _input->node.focused();
    if (focused || ++_focus_frames > 30) {
      _focus_pending = false;
    } else if (_input != nullptr) {
      _input->request_focus();
    }
  }
  const int count = static_cast<int>(store().items().size());
  if (count != _seen) {
    _seen = count;
    _hold = PEEK_HOLD_MS;
    _alpha = 1.0;
    node.set_opacity(1.0);
    render();
  } else if (!_target_set || store().target() != _target) {
    render();
  }
  if (!store().peek) return;
  if (_hold > 0.0) {
    _hold -= dt;
    return;
  }
  _alpha -= dt / PEEK_FADE_MS;
  node.set_opacity(std::max(0.0, _alpha));
  if (_alpha <= 0.0) close();
}

void ChatLogic::on_key_down(LFWKeyEvent& e) {
  if (e.key != u"escape") return;
  if (store().peek) {
    close();
  } else {
    exit_input();
  }
  e.stop_immediate_propagation();
}

void ChatLogic::on_stop() {
  blur_input();
  store().peek = false;
  _closing = true;
  if (_input != nullptr) {
    _input->on_submit = nullptr;
    _input->on_cancel = nullptr;
  }
}

void ChatLogic::send() {
  if (_input == nullptr) return;
  if (!store().send(_input->text())) return;
  _locked = true;
  _input->set_text(std::u16string());
  _hold = 0.0;
  _alpha = 1.0;
  node.set_opacity(1.0);
  store().peek = false;
  set_input_visible(true);
  render();
  keep_focus();
}

void ChatLogic::keep_focus() {
  _focus_pending = true;
  _focus_frames = 0;
  if (_input != nullptr) _input->request_focus();
}

void ChatLogic::exit_input() {
  if (_closing || store().peek) return;
  _locked = false;
  store().peek = true;
  _hold = PEEK_HOLD_MS;
  _alpha = 1.0;
  node.set_opacity(1.0);
  set_input_visible(false);
  render();
}

void ChatLogic::close() {
  if (_closing) return;
  _closing = true;
  blur_input();
  node.set_opacity(1.0);
  node.pop_page(UIPopPageOpts{});
}

void ChatLogic::focus_input() {
  _locked = true;
  if (store().peek) {
    store().peek = false;
    _hold = 0.0;
    _alpha = 1.0;
    node.set_opacity(1.0);
    set_input_visible(true);
    render();
  }
  _focus_pending = true;
  _focus_frames = 0;
}

std::u16string ChatLogic::channel_label(net::ChatTarget target) {
  const Value label =
      lfw().string(Value(std::u16string(target == net::ChatTarget::kRoom ? u"network.room_chat"
                                                                        : u"network.global")));
  const std::u16string* const s = std::get_if<std::u16string>(&label);
  std::u16string text = u"[";
  if (s != nullptr) text += *s;
  text += u"]";
  return text;
}

void ChatLogic::set_input_visible(bool visible) {
  UINode* const bg = q(u"chat_bg");
  const bool changed = bg == nullptr || bg->self_visible() != visible;
  if (bg != nullptr) bg->set_visible(visible);
  if (UINode* const input_bg = q(u"chat_input_bg")) input_bg->set_visible(visible);
  if (UINode* const divider = q(u"chat_input_divider")) divider->set_visible(visible);
  if (!visible) {
    blur_input();
  } else if (changed && !store().peek) {
    _focus_pending = true;
  }
}

void ChatLogic::blur_input() {
  if (_input != nullptr) {
    _input->node.blur();
    _input->request_blur();
  }
  _focus_pending = false;
}

double ChatLogic::measure(const std::u16string& text, UINode* node) {
  if (node == nullptr) return 0.0;
  const Value ti = lfw().host().measure_text(Value(text), node->style.data());
  const Value* const w = field_of(ti, u"w");
  if (w == nullptr) return 0.0;
  const double width = to_number(*w);
  return std::isfinite(width) ? width : 0.0;
}

std::u16string ChatLogic::fit(const std::u16string& text, double max_w, UINode* node) {
  if (max_w <= 8.0) return std::u16string();
  if (measure(text, node) <= max_w) return text;
  const std::vector<std::u16string> chars = code_points(text);
  size_t lo = 0;
  size_t hi = chars.size();
  while (lo < hi) {
    const size_t mid = (lo + hi + 1) / 2;
    if (measure(join(chars, 0, mid) + ELLIPSIS, node) <= max_w) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return join(chars, 0, lo) + ELLIPSIS;
}

std::u16string ChatLogic::wrap_lines(const std::u16string& text, double max_w, UINode* node,
                                     int max_lines) {
  if (max_w <= 8.0 || text.empty()) return std::u16string();
  if (measure(text, node) <= max_w) return text;
  const std::vector<std::u16string> chars = code_points(text);
  std::vector<std::u16string> lines;
  size_t i = 0;
  while (i < chars.size() && static_cast<int>(lines.size()) < max_lines) {
    while (i < chars.size() && chars[i] == u" ") ++i;
    if (i >= chars.size()) break;
    const bool last = static_cast<int>(lines.size()) == max_lines - 1;
    size_t lo = 1;
    size_t hi = chars.size() - i;
    while (lo < hi) {
      const size_t mid = (lo + hi + 1) / 2;
      const std::u16string s = join(chars, i, i + mid);
      if (measure(last ? s + ELLIPSIS : s, node) <= max_w) {
        lo = mid;
      } else {
        hi = mid - 1;
      }
    }
    size_t take = std::max<size_t>(1, lo);
    if (i + take < chars.size() && !last) {
      for (size_t k = take - 1; k >= take / 2; --k) {
        if (chars[i + k] == u" ") {
          take = k + 1;
          break;
        }
        if (k == 0) break;
      }
      while (take > 1 && contains(NO_LINE_START, char_at(chars, i + take))) --take;
      while (take > 1 && contains(NO_LINE_END, char_at(chars, i + take - 1))) --take;
    }
    std::u16string line = join(chars, i, i + take);
    i += take;
    if (i < chars.size()) {
      line = trim_end(line);
      if (last) line += ELLIPSIS;
    }
    lines.push_back(std::move(line));
  }
  std::u16string out;
  for (size_t k = 0; k < lines.size(); ++k) {
    if (k != 0) out += u"\n";
    out += lines[k];
  }
  return out;
}

double ChatLogic::render_row(Row& row, const net::ChatItem& item) {
  UINode* const name = row.name;
  UINode* const body = row.body;
  if (name == nullptr || body == nullptr) return ROW_H;
  if (!item.sys) {
    set_fill(name, channel_color(item.target));
    name->set_visible(true);
    const std::u16string sender = item.sender_name.empty() ? std::u16string(u"?") : item.sender_name;
    const std::u16string name_text = channel_label(item.target) + u" " + sender + u":";
    name->set_text(fit(name_text, ROW_W * NAME_MAX_RATIO, name));
    set_fill(body, BODY_COLOR);
    const double body_w = ROW_W - name->w() - NAME_GAP;
    body->set_text(wrap_lines(item.text, body_w, body, MAX_LINES));
    return std::max({name->h(), body->h(), ROW_H});
  }
  name->set_visible(false);
  set_fill(body, SYSTEM_COLOR);
  body->set_text(wrap_lines(item.text, ROW_W, body, MAX_LINES));
  return std::max(body->h(), ROW_H);
}

void ChatLogic::render() {
  const std::vector<net::ChatItem>& items = store().items();
  const bool typing = !store().peek;
  node.set_ignore_key(!typing);
  double used = 0.0;
  int slot = static_cast<int>(_rows.size()) - 1;
  for (int i = static_cast<int>(items.size()) - 1; i >= 0 && slot >= 0; --i) {
    if (used + ROW_H > LIST_MAX_H) break;
    UINode* const row_node = _list != nullptr ? _list->children()[static_cast<size_t>(slot)]
                                              : nullptr;
    const double row_h = std::ceil(render_row(_rows[static_cast<size_t>(slot)], items[i]));
    if (row_node != nullptr) {
      row_node->resize(ROW_W, row_h);
      row_node->set_visible(true);
    }
    used += row_h + ROW_GAP;
    --slot;
  }
  for (int i = slot; i >= 0; --i) {
    if (_list == nullptr) break;
    if (UINode* const row_node = _list->children()[static_cast<size_t>(i)]) {
      row_node->set_visible(false);
    }
  }

  if (_list != nullptr) {
    _list->resize(ROW_W, used).move_to(LIST_LEFT, typing ? LIST_BOTTOM_TYPING : LIST_BOTTOM_PEEK);
  }
  if (typing) {
    if (UINode* const bg = q(u"chat_bg")) bg->resize(BG_W, used + BG_EXTRA_H);
  }

  const net::ChatTarget target = store().target();
  _target = target;
  _target_set = true;
  if (UINode* const tag = q(u"chat_target")) {
    set_fill(tag, channel_color(target));
    tag->set_text(channel_label(target));
  }
  layout();
}

void ChatLogic::layout() {
  if (_list == nullptr) return;
  for (UINode* const row : _list->children()) {
    if (row == nullptr || !row->visible()) continue;
    UIComponent* const flex = row->find_component(Flex::class_tag());
    if (flex != nullptr && is_class(flex->clazz(), Flex::class_tag())) {
      static_cast<Flex*>(flex)->apply();
    }
  }
  UIComponent* const flex = _list->find_component(Flex::class_tag());
  if (flex != nullptr && is_class(flex->clazz(), Flex::class_tag())) {
    static_cast<Flex*>(flex)->apply();
  }
}

}  // namespace ui
}  // namespace lfw
