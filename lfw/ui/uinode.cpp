#include "lfw/ui/uinode.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "lfw/core/js_string.h"
#include "lfw/core/json.h"
#include "lfw/lfw.h"
#include "lfw/ui/action/actor.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/ui/register_class.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"
#include "lfw/utils/string_help.h"

namespace lfw::ui {

std::unordered_set<const UINode*>& live_uinodes() {
  static std::unordered_set<const UINode*>* const nodes = new std::unordered_set<const UINode*>();
  return *nodes;
}

namespace {

const bool s_registered = []() {
  regist_ui_class(UINode::class_tag(), u"UINode");
  return true;
}();

bool nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

Value opt_text_value(const std::optional<std::u16string>& v) {
  return v.has_value() ? Value(*v) : Value();
}

Value opt_number_value(const std::optional<double>& v) {
  return v.has_value() ? Value(*v) : Value();
}

double num_or_zero(const Value& v) {
  const double n = to_number(v);
  return truthy(Value(n)) ? n : 0.0;
}

double num_or_one(const Value& v) {
  const double n = to_number(v);
  return truthy(Value(n)) ? n : 1.0;
}

bool loose_str_equals(const Value& v, const std::u16string& text) {
  if (const std::u16string* const s = std::get_if<std::u16string>(&v)) return *s == text;
  if (std::holds_alternative<double>(v) || std::holds_alternative<bool>(v)) {
    return to_string(v) == text;
  }
  return false;
}

// ---------- alignment（移植自 src/LFW/ui/alignment.ts 的 apply 部分） ----------

std::u16string align_text(const Value* const v) {
  if (v == nullptr) return std::u16string();
  const std::u16string* const s = std::get_if<std::u16string>(v);
  if (s == nullptr) return std::u16string();
  return to_lower_case(js_trim(*s));
}

/// `50%` → 0.5；不是百分比返回 nullopt
std::optional<double> align_percent(const std::u16string& text) {
  if (text.size() < 2 || text.back() != u'%') return std::nullopt;
  const double n = string_to_number(js_trim(text.substr(0, text.size() - 1)));
  if (std::isnan(n)) return std::nullopt;
  return n / 100.0;
}

double align_num(const Object& o, const char16_t* const key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return 0.0;
  const double n = to_number(*v);
  return std::isnan(n) ? 0.0 : n;
}

}

UINode::~UINode() { live_uinodes().erase(this); }

UINode::UINode(LFW& lfw, const Value& data, UINode* parent, UILayer* layer)
    : _lfw(&lfw), _data(data), _info(data), _parent(parent), _layer(layer) {
  live_uinodes().insert(this);
  _root = parent != nullptr ? &parent->root() : this;
  _disabled = _info.disabled().value_or(false);
  _visible = _info.visible().value_or(true);
  _clip_children = _info.clips().value_or(false);
  _local_only = _info.local_only();
  _ignore_esc = _info.ignore_esc().value_or(false);
  _touchable = _info.touchable();
  _alignment = _info.alignment();
  _follow_screen = _info.follow_screen();
  _opacity = Value(_info.opacity().value_or(1.0));
  center = _info.center();
  pos = _info.pos();
  size = _info.size();
  scale = _info.scale();
  // 与 TS 的 `stretchable = !data.size` 等价：UI 没写 size、也不是靠图/屏幕撑出来的，才按文本自适应
  const Value* const raw_size = field_of(_info.raw(), u"size");
  const bool has_size = (raw_size != nullptr && truthy(*raw_size)) || !nullish(_info.img_info()) ||
                        parent == nullptr;
  stretchable = !has_size;
  // 样式要先于文本挂上：文本测量、i18n 重解析都按节点样式来
  if (truthy(_info.style())) style.assign(_info.style());
  {
    const Value* const flex_v = field_of(_data, u"flex");
    if (flex_v != nullptr) flex.assign(*flex_v);
    flex.on_change = [this]() { on_flex_change(); };
  }
  {
    Value tv = !nullish(_info.txt_info()) ? _info.txt_info() : Value(NullTag{});
    const bool has_i18n = _info.i18n().has_value() && !_info.i18n()->empty();
    if (has_i18n) {
      _i18n_key = *_info.i18n();
      // 有烤好的测量结果就直接换语言重测，没有就按 key 现测一次
      tv = truthy(tv) ? make_i18n_text(tv, *_info.i18n())
                      : _lfw->host().measure_text(_lfw->string(Value(*_info.i18n())),
                                                  style.data());
    }
    set_text_object(tv);
  }
  {
    const Value& img = _info.img_info();
    _image = !nullish(img) ? img : Value(NullTag{});
  }
  if (_info.color().has_value()) color = *_info.color();
  _outlineColor = opt_text_value(_info.outlineColor());
  _outlineWidth = opt_number_value(_info.outlineWidth());
  _outlineAlpha = opt_number_value(_info.outlineAlpha());
  _owned_renderer.reset(lfw.host().create_ui_node_renderer(*this));
  renderer = _owned_renderer.get();
  if (renderer != nullptr) renderer->ui = this;
}

Value UINode::make_i18n_text(const Value& baked, const std::u16string& key) {
  const Value resolved = _lfw->string(Value(key));
  const Value* const baked_text = field_of(baked, u"text");
  if (baked_text != nullptr && strict_equals(resolved, *baked_text)) return baked;
  return _lfw->host().measure_text(resolved, style.data());
}

UINode& UINode::set_text_object(const Value& v) {
  _text = v;
  auto_size_by_text(v);
  return *this;
}

UINode& UINode::set_text(const std::u16string& text) {
  // 代码指定的文本接管显示，解除 i18n 绑定
  _i18n_key.reset();
  const Value* const prev = field_of(_text, u"text");
  if (prev != nullptr && strict_equals(*prev, Value(text))) {
    auto_size_by_text(_text);
    return *this;
  }
  Value obj(std::make_shared<Object>());
  Object* const o = as_object(obj);
  o->set(u"text", Value(text));
  o->set(u"w", Value(0.0));
  o->set(u"h", Value(0.0));
  o->set(u"scale", Value(0.0));
  _text = obj;
  auto_size_by_text(_text);
  return *this;
}

UINode& UINode::set_i18n(const std::u16string& key) {
  if (key.empty()) {
    _i18n_key.reset();
    return *this;
  }
  _i18n_key = key;
  const Value resolved = _lfw->string(Value(key));
  const Value* const cur = field_of(_text, u"text");
  if (cur != nullptr && strict_equals(*cur, resolved)) {
    refresh_text_layout();
    return *this;
  }
  _text = _lfw->host().measure_text(resolved, style.data());
  auto_size_by_text(_text);
  return *this;
}

UINode& UINode::clear_i18n() {
  _i18n_key.reset();
  return *this;
}

std::u16string UINode::resolve_text(const Value& v) const {
  std::u16string source;
  if (_i18n_key.has_value()) {
    source = *_i18n_key;
  } else {
    const Value* const text_v = field_of(v, u"text");
    if (text_v != nullptr) source = to_string(*text_v);
  }
  if (source.empty()) return std::u16string();
  return to_string(_lfw->string(Value(source)));
}

void UINode::refresh_text_layout() {
  _auto_size_key = TextSizeKey();
  auto_size_by_text(_text);
}

UINode& UINode::refresh_lang_text() {
  if (_i18n_key.has_value()) {
    _text = _lfw->host().measure_text(_lfw->string(Value(*_i18n_key)), style.data());
    auto_size_by_text(_text);
    return *this;
  }
  if (truthy(_text)) refresh_text_layout();
  return *this;
}

bool UINode::same_text_size_key(const TextSizeKey& a, const TextSizeKey& b) const {
  return a.text == b.text && a.version == b.version;
}

UINode::TextSizeKey UINode::text_size_key(const Value& v) const {
  TextSizeKey k;
  k.set = true;
  k.text = resolve_text(v);
  k.version = style.version();
  return k;
}

void UINode::auto_size_by_text(const Value& v) {
  if (!truthy(v) || !stretchable || _parent == nullptr) {
    _auto_size_key = TextSizeKey();
    return;
  }
  const TextSizeKey key = text_size_key(v);
  if (_auto_size_key.set && same_text_size_key(_auto_size_key, key)) return;
  _auto_size_key = key;
  const Value ti = _lfw->host().measure_text(Value(resolve_text(v)), style.data());
  const Value* const tw = field_of(ti, u"w");
  const Value* const th = field_of(ti, u"h");
  const Value* const ts = field_of(ti, u"scale");
  const double w = tw != nullptr ? num_or_zero(*tw) : 0.0;
  const double h = th != nullptr ? num_or_zero(*th) : 0.0;
  const double s = ts != nullptr ? num_or_one(*ts) : 1.0;
  resize(w / s, h / s);
}

void UINode::clear_caches() {
  _cache_cross.reset();
  _cache_rect.reset();
  _cache_geo.reset();
  _cache_global_pos.reset();
}

UINode& UINode::set_w(double v) { return resize(v, h()); }
UINode& UINode::set_h(double v) { return resize(w(), v); }

UINode& UINode::resize(std::optional<double> x, std::optional<double> y, std::optional<double> z) {
  size.x = round_float(x.value_or(size.x));
  size.y = round_float(y.value_or(size.y));
  size.z = round_float(z.value_or(size.z));
  clear_caches();
  if (is_flex()) _flex_dirty = true;
  if (_parent != nullptr && !_parent->_applying_flex) _parent->invalidate_flex();
  return *this;
}

UINode& UINode::set_x(double v) { return move_to(v); }
UINode& UINode::set_y(double v) { return move_to({}, v); }
UINode& UINode::set_z(double v) { return move_to({}, {}, v); }

UINode& UINode::move_to(std::optional<double> x, std::optional<double> y, std::optional<double> z) {
  pos.x = round_float(x.value_or(pos.x));
  pos.y = round_float(y.value_or(pos.y));
  pos.z = round_float(z.value_or(pos.z));
  clear_caches();
  return *this;
}

UINode& UINode::set_cx(double v) { return set_center(v); }
UINode& UINode::set_cy(double v) { return set_center({}, v); }
UINode& UINode::set_cz(double v) { return set_center({}, {}, v); }

UINode& UINode::set_center(std::optional<double> x, std::optional<double> y,
                           std::optional<double> z) {
  center.x = round_float(x.value_or(center.x));
  center.y = round_float(y.value_or(center.y));
  center.z = round_float(z.value_or(center.z));
  clear_caches();
  if (_parent != nullptr) _parent->invalidate_flex();
  return *this;
}

UINode& UINode::set_sx(double v) { return set_scale(v); }
UINode& UINode::set_sy(double v) { return set_scale({}, v); }
UINode& UINode::set_sz(double v) { return set_scale({}, {}, v); }

UINode& UINode::set_scale(std::optional<double> x, std::optional<double> y,
                          std::optional<double> z) {
  scale.x = round_float(x.value_or(scale.x));
  scale.y = round_float(y.value_or(scale.y));
  scale.z = round_float(z.value_or(scale.z));
  return *this;
}

const Vector3& UINode::global_pos() const {
  if (_cache_global_pos.has_value()) return *_cache_global_pos;
  Vector3 out = pos;
  if (_parent != nullptr) {
    const Vector3& g = _parent->global_pos();
    out.x += g.x;
    out.y += g.y;
    out.z += g.z;
  }
  _cache_global_pos = out;
  return *_cache_global_pos;
}

void UINode::set_global_pos(double x, double y, double z) {
  if (_parent == nullptr) {
    move_to(x, y, z);
    return;
  }
  const Vector3& g = _parent->global_pos();
  move_to(x - g.x, y - g.y, z - g.z);
}

UINode& UINode::move_to_global(double x, double y, double z) {
  if (_parent == nullptr) return move_to(x, y, z);
  const Vector3& g = _parent->global_pos();
  return move_to(x - g.x, y - g.y, z - g.z);
}

const UINode::Cross& UINode::cross() const {
  if (_cache_cross.has_value()) return *_cache_cross;
  const double w_ = size.x;
  const double h_ = size.y;
  const double a = center.x;
  const double b = center.y;
  Cross out;
  out.left = -a * w_;
  out.top = -b * h_;
  out.right = (1 - a) * w_;
  out.bottom = (1 - b) * h_;
  out.mid_x = (out.left + out.right) / 2;
  out.mid_y = (out.top + out.bottom) / 2;
  _cache_cross = out;
  return *_cache_cross;
}

const UINode::Rect& UINode::rect() const {
  if (_cache_rect.has_value()) return *_cache_rect;
  const Cross& c = cross();
  Rect out;
  out.left = pos.x + c.left;
  out.top = pos.y + c.top;
  out.right = pos.x + c.right;
  out.bottom = pos.y + c.bottom;
  _cache_rect = out;
  return *_cache_rect;
}

const UINode::Geo& UINode::geo() const {
  if (_cache_geo.has_value()) return *_cache_geo;
  const Cross& c = cross();
  const Vector3& g = global_pos();
  Geo out;
  out.pos_x = g.x;
  out.pos_y = g.y;
  out.left = g.x + c.left;
  out.top = g.y + c.top;
  out.right = g.x + c.right;
  out.bottom = g.y + c.bottom;
  _cache_geo = out;
  return *_cache_geo;
}

bool UINode::hit(double x, double y) const {
  const double l = pos.x - lfw::round(center.x * size.x);
  const double t = pos.y - lfw::round(center.y * size.y);
  const double dw = info().size().x;
  const double dh = info().size().y;
  return l <= x && t <= y && l + dw >= x && t + dh >= y;
}

void UINode::set_focused(bool v) {
  if (v == (focused_node() == this)) return;
  if (v) {
    set_focused_node(this);
  } else if (focused_node() == this) {
    set_focused_node(nullptr);
  }
}

void UINode::set_focused_node(UINode* val) {
  UINode& rt = root();
  UINode* const old = rt._focused_node;
  if (old == val) return;
  if (val != nullptr && (val->disabled() || !val->visible())) val = nullptr;
  rt._focused_node = val;
  if (old != nullptr) {
    old->on_blur();
    if (old->callbacks.on_foucs_changed) old->callbacks.on_foucs_changed(*old);
  }
  if (val != nullptr) {
    val->on_foucs();
    if (val->callbacks.on_foucs_changed) val->callbacks.on_foucs_changed(*val);
  }
  if (rt.callbacks.on_foucs_item_changed) rt.callbacks.on_foucs_item_changed(val, old);
}

Value UINode::id() const { return info().id_value(); }

Value UINode::name() const { return info().name_value(); }

bool UINode::visible() const {
  if (_parent == nullptr) return _visible;
  return _parent->visible() && _visible;
}

UINode& UINode::set_visible(bool v) {
  const bool prev = visible();
  _visible = v;
  if (prev != visible()) {
    invoke_all_visible();
    if (_parent != nullptr) _parent->invalidate_flex();
  }
  if (!v && focused_node() != nullptr && !focused_node()->visible()) set_focused_node(nullptr);
  return *this;
}

std::u16string UINode::background() const {
  if (_background.has_value()) return *_background;
  if (info().background().has_value()) return *info().background();
  return u"#000000";
}

std::u16string UINode::foreground() const {
  if (_foreground.has_value()) return *_foreground;
  if (info().foreground().has_value()) return *info().foreground();
  return u"#000000";
}

double UINode::backgroundAlpha() const {
  if (_backgroundAlpha.has_value()) return *_backgroundAlpha;
  return info().backgroundAlpha().value_or(0.0);
}

double UINode::foregroundAlpha() const {
  if (_foregroundAlpha.has_value()) return *_foregroundAlpha;
  return info().foregroundAlpha().value_or(0.0);
}

bool UINode::disabled() const {
  if (_parent == nullptr) return _disabled;
  return _parent->disabled() || _disabled;
}

double UINode::global_opacity() const {
  if (_parent == nullptr) return to_number(_opacity);
  return to_number(_opacity) * to_number(_parent->_opacity);
}

double UINode::opacity() const { return to_number(_opacity); }

UINode& UINode::set_opacity(double v) {
  _opacity = Value(v);
  return *this;
}

UINode& UINode::set_disabled(bool v) {
  _disabled = v;
  if (v && focused_node() != nullptr && focused_node()->disabled()) set_focused_node(nullptr);
  return *this;
}

UINode& UINode::add_child(UINode& node) {
  _children.push_back(&node);
  invalidate_flex();
  return *this;
}

UINode& UINode::add_children(const std::vector<UINode*>& nodes) {
  for (UINode* const n : nodes) add_child(*n);
  return *this;
}

bool UINode::is_flex() const { return flex.is_flex(); }

void UINode::invalidate_flex() {
  if (_flex_dirty) return;
  _flex_dirty = true;
  if (_parent != nullptr) _parent->invalidate_flex();
}

void UINode::flush_flex() {
  if (!_flex_dirty) return;
  _flex_dirty = false;
  if (is_flex()) {
    const double w = this->w();
    const double h = this->h();
    _applying_flex = true;
    apply_flex_layout(*this);
    _applying_flex = false;
    _flex_dirty = false;
    if (this->w() != w || this->h() != h) {
      if (_parent != nullptr) _parent->invalidate_flex();
    }
  }
  for (UINode* const child : _children) {
    if (child != nullptr) child->flush_flex();
  }
}

void UINode::on_flex_change() {
  if (is_flex()) _flex_dirty = true;
  if (_parent != nullptr) _parent->invalidate_flex();
}

UINode* UINode::find_child(const std::u16string& id) {
  for (UINode* const c : _children) {
    if (loose_str_equals(c->id(), id)) return c;
  }
  return nullptr;
}

UINode* UINode::find_child_by_name(const std::u16string& name) {
  for (UINode* const c : _children) {
    if (loose_str_equals(c->name(), name)) return c;
  }
  return nullptr;
}

UINode* UINode::search_node(const std::u16string& id) {
  UINode* ret = find_child(id);
  if (ret != nullptr) return ret;
  for (UINode* const c : _children) {
    ret = c->search_node(id);
    if (ret != nullptr) return ret;
  }
  return nullptr;
}

UINode* UINode::lookup_node(const std::u16string& id) {
  UINode* const ret = find_child(id);
  if (ret != nullptr) return ret;
  return _parent != nullptr ? _parent->lookup_node(id) : nullptr;
}

UINode* UINode::find_parent_by_id(const std::u16string& id) {
  UINode* parent = _parent;
  while (parent != nullptr) {
    if (loose_str_equals(parent->id(), id)) return parent;
    parent = parent->_parent;
  }
  return nullptr;
}

UINode* UINode::find_parent(
    const std::function<bool(UINode&)>& match,
    const std::function<void(UINode&, const std::vector<UINode*>&)>& handler) {
  UINode* parent = _parent;
  std::vector<UINode*> path;
  while (parent != nullptr) {
    path.push_back(parent);
    if (match(*parent)) {
      if (handler) handler(*parent, path);
      return parent;
    }
    parent = parent->_parent;
  }
  return nullptr;
}

Value UINode::get_value(const std::u16string& name, bool lookup) const {
  const Object* const o = as_object(info().values());
  if (o != nullptr) {
    if (const Value* const v = o->get(name)) return *v;
  }
  if (lookup && _parent != nullptr) return _parent->get_value(name, lookup);
  return Value();
}

void UINode::on_pointer_down(LFWPointerEvent& e) {
  const int button = static_cast<int>(e.button);
  if (button >= 0 && button < static_cast<int>(_pointer_down.size())) _pointer_down[button] = 1;
  _click_flag = 1;
  for (UIComponent* const c : _components) c->on_pointer_down(e);
  if (callbacks.on_pointer_down) callbacks.on_pointer_down(e, *this);
}

void UINode::on_pointer_move(LFWPointerEvent& e) {
  for (UIComponent* const c : _components) c->on_pointer_move(e);
  if (callbacks.on_pointer_move) callbacks.on_pointer_move(e, *this);
}

void UINode::on_pointer_up(LFWPointerEvent& e) {
  const int button = static_cast<int>(e.button);
  if (button >= 0 && button < static_cast<int>(_pointer_down.size())) _pointer_down[button] = 0;
  for (UIComponent* const c : _components) c->on_pointer_up(e);
  if (callbacks.on_pointer_up) callbacks.on_pointer_up(e, *this);
}

void UINode::on_pointer_cancel(LFWPointerEvent& e) {
  const int button = static_cast<int>(e.button);
  if (button >= 0 && button < static_cast<int>(_pointer_down.size())) _pointer_down[button] = 0;
  for (UIComponent* const c : _components) c->on_pointer_cancel(e);
  if (callbacks.on_pointer_cancel) callbacks.on_pointer_cancel(e, *this);
}

bool UINode::is_pointer_down(int button) const {
  if (button < 0 || button >= static_cast<int>(_pointer_down.size())) return false;
  return _pointer_down[static_cast<size_t>(button)] != 0;
}

bool UINode::touchable() const {
  if (_touchable.has_value()) return *_touchable;
  return _parent != nullptr ? _parent->touchable() : true;
}

bool UINode::local_only() const {
  if (_local_only.has_value()) return *_local_only;
  return _parent != nullptr ? _parent->local_only() : false;
}

void UINode::on_pointer_leave() {
  _pointer_over = 0;
  _click_flag = 0;
  for (UIComponent* const c : _components) c->on_pointer_leave();
  if (callbacks.on_pointer_leave) callbacks.on_pointer_leave(*this);
}

void UINode::on_pointer_enter() {
  _pointer_over = 1;
  for (UIComponent* const c : _components) c->on_pointer_enter();
  if (callbacks.on_pointer_enter) callbacks.on_pointer_enter(*this);
}

void UINode::on_show() {
  for (UIComponent* const c : _components) c->on_show();
  if (callbacks.on_show) callbacks.on_show(*this);
  if (info().auto_focus().value_or(false) && !disabled() && focused_node() == nullptr) {
    set_focused_node(this);
  }
  if (renderer != nullptr) renderer->on_show();
}

void UINode::on_hide() {
  if (focused_node() == this) set_focused_node(nullptr);
  for (UIComponent* const c : _components) c->on_hide();
  if (callbacks.on_hide) callbacks.on_hide(*this);
  if (renderer != nullptr) renderer->on_hide();
}

void UINode::on_foucs() {
  for (UIComponent* const c : _components) c->on_foucs();
  if (renderer != nullptr) renderer->on_foucs();
}

void UINode::on_blur() {
  for (UIComponent* const c : _components) c->on_blur();
  if (renderer != nullptr) renderer->on_blur();
}

void UINode::invoke_all_on_show() {
  on_show();
  for (UINode* const child : _children) {
    if (child->_visible) child->invoke_all_on_show();
  }
}

void UINode::invoke_all_on_hide() {
  on_hide();
  for (UINode* const child : _children) {
    if (child->_visible) child->invoke_all_on_hide();
  }
}

void UINode::invoke_all_visible() {
  if (_visible) {
    invoke_all_on_show();
  } else {
    invoke_all_on_hide();
  }
}

UINode* UINode::find_alignment_node(const std::u16string& which_in) {
  const std::u16string which = js_trim(which_in);
  if (which.empty() || which == u"parent") return _parent;
  if (which == u"self") return this;
  if (which.rfind(u"parent:", 0) == 0) {
    const double distance = to_number(Value(which.substr(7)));
    if (std::isnan(distance)) return _parent;
    UINode* p = _parent;
    for (double i = 0.0; i < distance && p != nullptr; i += 1.0) p = p->parent();
    return p;
  }
  if (which.rfind(u"bro:", 0) == 0) {
    if (_parent == nullptr) return nullptr;
    const std::u16string v = js_trim(which.substr(4));
    const std::vector<UINode*>& siblings = _parent->children();
    const auto it = std::find(siblings.begin(), siblings.end(), this);
    if (it == siblings.end()) return nullptr;
    const ptrdiff_t idx = it - siblings.begin();
    if (v == u"prev" || v == u"-1") {
      return idx > 0 ? siblings[static_cast<size_t>(idx - 1)] : nullptr;
    }
    if (v == u"next" || v == u"+1") {
      return idx + 1 < static_cast<ptrdiff_t>(siblings.size())
                 ? siblings[static_cast<size_t>(idx + 1)]
                 : nullptr;
    }
    const double n = to_number(Value(v));
    if (std::isnan(n)) return nullptr;
    const ptrdiff_t target = static_cast<ptrdiff_t>(n);
    if (target < 0 || target >= static_cast<ptrdiff_t>(siblings.size())) return nullptr;
    UINode* const ret = siblings[static_cast<size_t>(target)];
    return ret != this ? ret : nullptr;
  }
  if (which.rfind(u"id:", 0) == 0) return root().search_node(js_trim(which.substr(3)));
  if (which.rfind(u"name:", 0) == 0) return root().find_child_by_name(js_trim(which.substr(5)));
  return nullptr;
}

bool UINode::alignment_once() const {
  const Object* const a = as_object(_alignment);
  if (a == nullptr) return false;
  const Value* const v = a->get(u"once");
  if (v == nullptr) return false;
  if (const bool* const b = std::get_if<bool>(v)) return *b;
  if (const double* const d = std::get_if<double>(v)) return *d == 1.0;
  if (const std::u16string* const s = std::get_if<std::u16string>(v)) {
    return *s == u"1" || *s == u"true";
  }
  return false;
}

bool UINode::apply_alignment() {
  const Object* const a = as_object(_alignment);
  if (a == nullptr) return false;

  const Value* const followed_v = a->get(u"followed");
  const auto follow_of = [&](const char16_t* side) -> UINode* {
    std::u16string which = u"parent";
    if (followed_v != nullptr) {
      if (const std::u16string* const s = std::get_if<std::u16string>(followed_v)) {
        which = *s;
      } else if (const Object* const fo = as_object(*followed_v)) {
        const Value* const sub = fo->get(side);
        if (sub != nullptr && !nullish(*sub)) {
          if (const std::u16string* const s2 = std::get_if<std::u16string>(sub)) which = *s2;
        }
      }
    }
    return find_alignment_node(which);
  };

  const std::u16string left = align_text(a->get(u"left"));
  const std::u16string right = align_text(a->get(u"right"));
  const std::u16string top = align_text(a->get(u"top"));
  const std::u16string bottom = align_text(a->get(u"bottom"));

  double off_l = 0.0;
  double off_r = 0.0;
  double off_t = 0.0;
  double off_b = 0.0;
  if (const Value* const ov = a->get(u"offset"); ov != nullptr) {
    if (const Object* const oo = as_object(*ov)) {
      off_l = align_num(*oo, u"l");
      off_r = align_num(*oo, u"r");
      off_t = align_num(*oo, u"t");
      off_b = align_num(*oo, u"b");
    }
  }

  const double cx = center.x;
  const double cy = center.y;
  const Vector3 gp = global_pos();
  double x = gp.x;
  double y = gp.y;
  const double z = gp.z;
  double w = size.x;
  double h = size.y;
  bool has_l = false;
  bool has_r = false;
  bool has_t = false;
  bool has_b = false;
  double _l = 0.0;
  double _r = 0.0;
  double _t = 0.0;
  double _b = 0.0;

  if (!right.empty()) {
    if (UINode* const f = follow_of(u"r"); f != nullptr && f != this) {
      const double fx = f->global_pos().x;
      const Cross& c = f->cross();
      if (const std::optional<double> p = align_percent(right)) {
        _r = fx + c.left + *p * (c.right - c.left - w) + w;
        has_r = true;
      } else if (right == u"left") {
        _r = fx + c.left;
        has_r = true;
      } else if (right == u"right") {
        _r = fx + c.right;
        has_r = true;
      }
    }
  }

  if (!left.empty()) {
    if (UINode* const f = follow_of(u"l"); f != nullptr && f != this) {
      const double fx = f->global_pos().x;
      const Cross& c = f->cross();
      if (const std::optional<double> p = align_percent(left)) {
        _l = fx + c.left + *p * (c.right - c.left - w);
        has_l = true;
      } else if (left == u"left") {
        _l = fx + c.left;
        has_l = true;
      } else if (left == u"right") {
        _l = fx + c.right;
        has_l = true;
      } else if (left == u"mid") {
        _l = fx + c.mid_x - 0.5 * w;
        has_l = true;
        has_r = false;
      }
    }
  }

  if (!bottom.empty()) {
    if (UINode* const f = follow_of(u"b"); f != nullptr && f != this) {
      const double fy = f->global_pos().y;
      const Cross& c = f->cross();
      if (const std::optional<double> p = align_percent(bottom)) {
        _b = fy + c.top + *p * (c.bottom - c.top - h) + h;
        has_b = true;
      } else if (bottom == u"top") {
        _b = fy + c.top;
        has_b = true;
      } else if (bottom == u"bottom") {
        _b = fy + c.bottom;
        has_b = true;
      }
    }
  }

  if (!top.empty()) {
    if (UINode* const f = follow_of(u"t"); f != nullptr && f != this) {
      const double fy = f->global_pos().y;
      const Cross& c = f->cross();
      if (const std::optional<double> p = align_percent(top)) {
        _t = fy + c.top + *p * (c.bottom - c.top - h);
        has_t = true;
      } else if (top == u"top") {
        _t = fy + c.top;
        has_t = true;
      } else if (top == u"bottom") {
        _t = fy + c.bottom;
        has_t = true;
      } else if (top == u"mid") {
        _t = fy + c.mid_y - 0.5 * h;
        has_t = true;
        has_b = false;
      }
    }
  }

  if (!has_l && !has_r && !has_t && !has_b) return false;
  if (has_l && has_r) w = (_r - _l) - off_l + off_r;
  if (has_l) {
    x = _l + cx * w + off_l;
  } else if (has_r) {
    x = _r - (1 - cx) * w + off_r;
  }
  if (has_t && has_b) h = (_b - _t) - off_t + off_b;
  if (has_t) {
    y = _t + cy * h + off_t;
  } else if (has_b) {
    y = _b - (1 - cy) * h + off_b;
  }
  move_to_global(x, y, z);
  resize(w, h);
  return true;
}

void UINode::update(double dt) {
  flush_flex();
  if (!_alignment_done && truthy(_alignment)) {
    if (apply_alignment() && alignment_once()) _alignment_done = true;
  }
  if (_prev_size.x != size.x || _prev_size.y != size.y || _prev_center.x != center.x ||
      _prev_center.y != center.y || _prev_pos.x != pos.x || _prev_pos.y != pos.y ||
      _prev_pos.z != pos.z) {
    clear_caches();
    _prev_size.x = size.x;
    _prev_size.y = size.y;
    _prev_center.x = center.x;
    _prev_center.y = center.y;
    _prev_pos.x = pos.x;
    _prev_pos.y = pos.y;
    _prev_pos.z = pos.z;
  }

  _update_times.add();
  _components_updating = true;
  for (UIComponent* const c : _components) {
    if (c->enabled()) c->update(dt);
  }
  _components_updating = false;
  if (!_del_components.empty()) {
    del_components(_del_components);
    _del_components.clear();
  }

  for (UINode* const child : _children) {
    child->flush_flex();
    if (!child->disabled()) child->update(dt);
  }
}

std::unique_ptr<UINode> UINode::create(LFW& lfw, const Value& info, UINode* parent,
                                       UILayer* layer) {
  std::unique_ptr<UINode> ret = std::make_unique<UINode>(lfw, info, parent, layer);
  {
    std::vector<std::unique_ptr<UIComponent>> components =
        lfw.factory.create_components(*ret, ret->info().component());
    for (std::unique_ptr<UIComponent>& c : components) {
      ret->_components.push_back(c.get());
      ret->_owned_components.push_back(std::move(c));
    }
  }
  const Value* const items = field_of(info, u"items");
  const Array* const arr = items != nullptr ? as_array(*items) : nullptr;
  if (arr != nullptr) {
    for (size_t i = 0; i < arr->size(); ++i) {
      const Value& item_info = arr->at(i);
      const Value* const count_p = field_of(item_info, u"count");
      double count = 1.0;
      if (count_p != nullptr && std::holds_alternative<double>(*count_p)) {
        const double c = std::get<double>(*count_p);
        if (!std::isnan(c) && c > 0.0) count = c;
      }
      while (count != 0.0 && !std::isnan(count)) {
        std::unique_ptr<UINode> child = create(lfw, item_info, ret.get());
        ret->add_child(*child);
        ret->_owned_children.push_back(std::move(child));
        count -= 1.0;
      }
    }
  }
  for (UIComponent* const c : ret->_components) c->on_add();
  ret->fire_singleton_links();
  ret->flush_flex();
  return ret;
}

void UINode::link_singleton(UIComponent& component) {
  for (UIComponent* const c : _linked_singletons) {
    if (c == &component) return;
  }
  _linked_singletons.push_back(&component);
}

void UINode::fire_singleton_links() {
  for (UIComponent* const c : _linked_singletons) {
    if (c != nullptr) c->on_link(*this);
  }
}

void UINode::on_start() {
  _update_times.reset();
  _state = Value(std::make_shared<Object>());
  _state_focused_node = nullptr;
  for (UIComponent* const c : _components) {
    c->stopped = false;
    c->on_start();
  }
  for (UINode* const c : _children) c->on_start();
  const std::vector<UIAction>* const start = info().actions_at(u"start");
  if (start != nullptr) actor().act(*this, *start);
  if (renderer != nullptr) renderer->on_start();
}

void UINode::on_stop() {
  for (UIComponent* const c : _components) {
    c->stopped = true;
    c->on_stop();
  }
  for (UIComponent* const c : _linked_singletons) {
    if (c != nullptr) c->on_unlink(*this);
  }
  _linked_singletons.clear();
  for (UINode* const c : _children) c->on_stop();
  const std::vector<UIAction>* const stop = info().actions_at(u"stop");
  if (stop != nullptr) actor().act(*this, *stop);
  if (renderer != nullptr) renderer->on_stop();
}

void UINode::on_resume() {
  flush_flex();
  if (_parent == nullptr) {
    set_focused_node(_state_focused_node);
    if (_visible) invoke_all_visible();
  }
  for (UIComponent* const c : _components) {
    c->paused = false;
    c->mounted = true;
    c->on_resume();
  }
  for (UINode* const c : _children) c->on_resume();
  const std::vector<UIAction>* const resume = info().actions_at(u"resume");
  if (resume != nullptr) actor().act(*this, *resume);
  if (renderer != nullptr) renderer->on_resume();
}

void UINode::on_pause() {
  if (_parent == nullptr) {
    _state_focused_node = focused_node();
    set_focused_node(nullptr);
    invoke_all_on_hide();
  }
  if (&root() == this && renderer != nullptr) renderer->del_self();
  const std::vector<UIAction>* const pause = info().actions_at(u"pause");
  if (pause != nullptr) actor().act(*this, *pause);
  for (UIComponent* const c : _components) {
    c->paused = true;
    c->mounted = false;
    c->on_pause();
    c->recycle_keys();
  }
  for (UINode* const c : _children) c->on_pause();
  if (renderer != nullptr) renderer->on_pause();
}

void UINode::on_click(LFWPointerEvent& e) {
  const std::vector<UIAction>* const click = info().actions_at(u"click");
  const std::vector<UIAction>* const rclick = info().actions_at(u"rclick");
  const std::vector<UIAction>* const mclick = info().actions_at(u"mclick");
  if (click != nullptr && e.button == 0.0) {
    actor().act(*this, *click);
    e.stop_propagation();
  }
  if (mclick != nullptr && e.button == 1.0) {
    actor().act(*this, *mclick);
    e.stop_propagation();
  }
  if (rclick != nullptr && e.button == 2.0) {
    actor().act(*this, *rclick);
    e.stop_propagation();
  }
  for (UIComponent* const c : _components) {
    c->on_click(e);
    if (e.stopped() == 2) break;
  }
  if (callbacks.on_click) callbacks.on_click(e);
}

void UINode::on_key_down(LFWKeyEvent& e) {
  if (e.stopped() != 0) return;
  for (UIComponent* const c : _components) {
    c->on_key_down(e);
    if (e.stopped() == 2) return;
  }
  for (UINode* const c : _children) {
    c->on_key_down(e);
    if (e.stopped() == 2) return;
  }
  const std::vector<UIAction>* const click = info().actions_at(u"click");
  const std::vector<UIAction>* const rclick = info().actions_at(u"rclick");
  const std::vector<UIAction>* const mclick = info().actions_at(u"mclick");
  if (focused() && e.game_key == u"a" && click != nullptr) {
    actor().act(*this, *click);
    e.stop_immediate_propagation();
  }
  if (focused() && e.game_key == u"j" && rclick != nullptr) {
    actor().act(*this, *rclick);
    e.stop_immediate_propagation();
  }
  if (focused() && e.game_key == u"d" && mclick != nullptr) {
    actor().act(*this, *mclick);
    e.stop_immediate_propagation();
  }
}

void UINode::on_key_up(LFWKeyEvent& e) {
  if (e.stopped() != 0) return;
  for (UINode* const c : _children) {
    c->on_key_up(e);
    if (e.stopped() == 2) return;
  }
  for (UIComponent* const c : _components) {
    c->on_key_up(e);
    if (e.stopped() == 2) return;
  }
}

const ClazzTag* UINode::class_tag() {
  static const ClazzTag tag;
  return &tag;
}

UINode& UINode::add_components(const std::vector<UIComponent*>& components) {
  for (UIComponent* const component : components) {
    bool exists = false;
    for (UIComponent* const c : _components) {
      if (c == component) {
        exists = true;
        break;
      }
    }
    if (exists) continue;
    _components.push_back(component);
    component->on_add();
    if (callbacks.on_component_add) callbacks.on_component_add(*component, *this);
  }
  return *this;
}

UINode& UINode::del_components(const std::vector<UIComponent*>& components) {
  if (_components_updating) {
    _del_components.insert(_del_components.end(), components.begin(), components.end());
    return *this;
  }
  for (UIComponent* const component : components) {
    const auto it = std::find(_components.begin(), _components.end(), component);
    if (it == _components.end()) continue;
    _components.erase(it);
    component->on_del();
    if (callbacks.on_component_del) callbacks.on_component_del(*component, *this);
  }
  return *this;
}

UIComponent* UINode::find_component(const ClazzTag* type,
                                    const std::function<bool(UIComponent&)>& condition) {
  for (UIComponent* const v : _components) {
    if (!is_class(v->clazz(), type)) continue;
    if (!condition) return v;
    if (condition(*v)) return v;
  }
  return nullptr;
}

UIComponent* UINode::find_component_by_id(const ClazzTag* type, const std::u16string& id) {
  for (UIComponent* const v : _components) {
    if (!is_class(v->clazz(), type)) continue;
    if (v->id == id) return v;
  }
  return nullptr;
}

std::vector<UIComponent*> UINode::find_components(
    const ClazzTag* type, const std::function<UIFind(UIComponent&)>& condition) {
  std::vector<UIComponent*> ret;
  for (UIComponent* const v : _components) {
    if (!is_class(v->clazz(), type)) continue;
    if (!condition) {
      ret.push_back(v);
      continue;
    }
    const UIFind r = condition(*v);
    if (r == UIFind::Abort) break;
    if (r == UIFind::Yes) ret.push_back(v);
    if (r == UIFind::End) break;
  }
  return ret;
}

std::vector<UIComponent*> UINode::find_components_by_id(const ClazzTag* type,
                                                        const std::u16string& id) {
  std::vector<UIComponent*> ret;
  for (UIComponent* const v : _components) {
    if (!is_class(v->clazz(), type)) continue;
    if (v->id == id) ret.push_back(v);
  }
  return ret;
}

UIComponent* UINode::search_component(const ClazzTag* type,
                                      const std::function<bool(UIComponent&)>& condition) {
  UIComponent* const ret = find_component(type, condition);
  if (ret != nullptr) return ret;
  for (UINode* const child : _children) {
    UIComponent* const found = child->search_component(type, condition);
    if (found != nullptr) return found;
  }
  return nullptr;
}

UIComponent* UINode::search_component_by_id(const ClazzTag* type, const std::u16string& id) {
  const auto cond = [&id](UIComponent& v) { return v.id == id; };
  return search_component(type, cond);
}

std::vector<UIComponent*> UINode::search_components(
    const ClazzTag* type, const std::function<UIFind(UIComponent&)>& condition) {
  std::vector<UIComponent*> ret = find_components(type, condition);
  for (UINode* const child : _children) {
    std::vector<UIComponent*> sub = child->search_components(type, condition);
    ret.insert(ret.end(), sub.begin(), sub.end());
  }
  return ret;
}

UIComponent* UINode::lookup_component(const ClazzTag* type,
                                      const std::function<bool(UIComponent&)>& condition) {
  UIComponent* const ret = find_component(type, condition);
  if (ret != nullptr) return ret;
  return _parent != nullptr ? _parent->lookup_component(type, condition) : nullptr;
}

UIComponent* UINode::lookup_component_by_id(const ClazzTag* type, const std::u16string& id) {
  const auto cond = [&id](UIComponent& v) { return v.id == id; };
  return lookup_component(type, cond);
}

bool UINode::traversal_components(const std::function<bool(UIComponent&, int)>& fn, int depth) {
  for (UIComponent* const c : _components) {
    if (fn(*c, depth)) return true;
  }
  for (UINode* const child : _children) {
    if (child->traversal_components(fn, depth + 1)) return true;
  }
  return false;
}

bool UINode::pop_page(const UIPopPageOpts& opts) {
  UINode& rt = root();
  UILayer* const layer = rt._layer;
  if (layer == nullptr) return false;
  if (layer->modal() == &rt) {
    UICloseModalOpts close;
    const Value id = rt.id();
    if (const std::u16string* const s = std::get_if<std::u16string>(&id)) {
      close.id = *s;
      close.has_id = true;
    }
    return layer->close_modal(close);
  }
  if (layer->ui() != &rt) return false;
  return layer->pop(opts);
}

}




