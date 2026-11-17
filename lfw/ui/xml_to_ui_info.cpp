#include "lfw/ui/xml_to_ui_info.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/core/json.h"
#include "lfw/utils/string_help.h"

namespace lfw::ui {

namespace {

std::vector<std::u16string> split_char(const std::u16string& s, char16_t sep) {
  std::vector<std::u16string> out;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == sep) {
      out.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  return out;
}

std::vector<std::u16string> split_trim(const std::u16string& s) {
  std::vector<std::u16string> out;
  for (const std::u16string& part : split_char(s, u',')) out.push_back(js_trim(part));
  return out;
}

Value arr_of(const std::vector<std::u16string>& xs) {
  auto a = std::make_shared<Array>();
  for (const std::u16string& x : xs) a->push_back(Value(x));
  return Value(a);
}

bool non_empty(const std::optional<std::u16string>& v) {
  return v.has_value() && !v->empty();
}Value str_or_undef(const std::optional<std::u16string>& v) {
  return v.has_value() ? Value(*v) : Value();
}

Value nums_or_undef(const std::optional<std::vector<double>>& ns) {
  if (!ns.has_value()) return Value();
  auto a = std::make_shared<Array>();
  for (double d : *ns) a->push_back(Value(d));
  return Value(a);
}

bool is_action_place(const std::u16string& tag) {
  return tag == u"click" || tag == u"resume" || tag == u"pause" || tag == u"start" ||
         tag == u"stop";
}

Value parse_action_children(const IXMLElement& place) {
  auto arr = std::make_shared<Array>();
  for (const IXMLElement* c : place.children()) {
    if (c->tag() == u"action") arr->push_back(Value(c->action_str()));
  }
  if (arr->size() != 0) return Value(arr);
  if (non_empty(place.attr(u"action")) || non_empty(place.attr(u"name"))) {
    return Value(place.action_str());
  }
  const std::u16string v = place.text();
  if (v.find(u',') != std::u16string::npos) return arr_of(split_trim(v));
  return Value(js_trim(v));
}

Value parse_component(const IXMLElement& el) {
  auto ret = std::make_shared<Object>();
  const std::optional<std::u16string> cls_attr = el.attr(u"cls");
  ret->set(u"cls", non_empty(cls_attr) ? Value(*cls_attr) : Value(el.tag()));
  const std::optional<std::u16string> args = el.attr(u"args");
  if (non_empty(args)) ret->set(u"args", arr_of(split_trim(*args)));
  const std::optional<std::u16string> id = el.attr(u"id");
  if (non_empty(id)) ret->set(u"id", Value(*id));
  const std::optional<std::u16string> weight = el.attr(u"weight");
  if (non_empty(weight)) ret->set(u"weight", to_number(Value(*weight)));
  for (const IXMLElement* c : el.children()) {
    if (c->tag() == u"properties") {
      ret->set(u"properties", c->as_object());
      break;
    }
  }
  return Value(ret);
}

}

namespace {

/** `flex="..."` 之外的简写属性，避免在 XML 里写一长串声明 */
const char16_t* const kFlexAttrs[] = {
    u"display",   u"direction",   u"wrap",  u"justify", u"align",
    u"align_content", u"align_self", u"gap", u"row_gap", u"col_gap",
    u"padding",   u"grow",        u"shrink", u"basis",  u"order",
    u"fit",       u"fit_w",       u"fit_h",
};

/** 元素属性 + 子元素 `<attrs>`：多个 `<attrs>` 按文档顺序覆盖，元素自身属性优先。 */
std::optional<std::u16string> attr_of(const IXMLElement& el, const char16_t* name) {
  const std::u16string key(name);
  for (const XmlAttr& a : el.attrs()) {
    if (a.name == key) return a.value;
  }
  const std::vector<IXMLElement*>& children = el.children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    const IXMLElement& child = **it;
    if (child.tag() != u"attrs") continue;
    for (const XmlAttr& a : child.attrs()) {
      if (a.name == key) return a.value;
    }
  }
  return std::nullopt;
}

/** 与 XML 元素的 `nums_attr` 一致：逗号分隔后逐段转数字 */
std::optional<std::vector<double>> nums_attr_of(const IXMLElement& el, const char16_t* name) {
  const std::optional<std::u16string> v = attr_of(el, name);
  if (!v.has_value()) return std::nullopt;
  std::vector<double> ret;
  for (const std::u16string& part : split_char(*v, u',')) {
    ret.push_back(to_number(Value(js_trim(part))));
  }
  return ret;
}

Value parse_flex_attrs(const IXMLElement& el) {
  const std::optional<std::u16string> inline_flex = attr_of(el, u"flex");
  if (inline_flex.has_value()) return Value(*inline_flex);
  Value out(std::make_shared<Object>());
  Object* const o = as_object(out);
  bool any = false;
  for (const char16_t* const name : kFlexAttrs) {
    const std::optional<std::u16string> v = attr_of(el, name);
    if (!v.has_value()) continue;
    o->set(std::u16string(name), Value(*v));
    any = true;
  }
  return any ? out : Value();
}

Value parse_flex_element(const IXMLElement& el) {
  const std::optional<std::u16string> inline_flex = attr_of(el, u"flex");
  if (non_empty(inline_flex)) return Value(*inline_flex);
  const std::optional<std::u16string> inline_value = attr_of(el, u"value");
  if (non_empty(inline_value)) return Value(*inline_value);
  const std::u16string text = js_trim(el.text());
  if (!text.empty()) return Value(text);
  for (const IXMLElement* c : el.children()) {
    if (c->tag() != u"properties") continue;
    const Value props = c->as_object();
    if (const Object* o = as_object(props); o != nullptr && o->size() != 0) return props;
    break;
  }
  return parse_flex_attrs(el);
}

/** `display:flex; gap:4` → 对象（按第一个 ':' 切分） */
Value flex_decls_value(const std::u16string& text) {
  auto out = std::make_shared<Object>();
  for (const std::u16string& part : split_char(text, u';')) {
    const std::u16string decl = js_trim(part);
    const size_t at = decl.find(u':');
    if (at == std::u16string::npos) continue;
    const std::u16string key = js_trim(decl.substr(0, at));
    const std::u16string value = js_trim(decl.substr(at + 1));
    if (key.empty() || value.empty()) continue;
    out->set(key, Value(value));
  }
  return Value(out);
}

/** 多个 flex 声明按文档顺序合并（后者覆盖前者）；只有一个时原样返回 */
Value merge_flex(const std::vector<Value>& specs) {
  if (specs.size() == 1) return specs[0];
  auto out = std::make_shared<Object>();
  for (const Value& spec : specs) {
    // `temp` 必须保活：`as_object` 返回的是它内部的裸指针
    Value temp;
    const Object* src = nullptr;
    if (const std::u16string* s = std::get_if<std::u16string>(&spec)) {
      temp = flex_decls_value(*s);
      src = as_object(temp);
    } else {
      src = as_object(spec);
    }
    if (src == nullptr) continue;
    for (const std::u16string& k : src->keys()) out->set(k, *src->get(k));
  }
  return Value(out);
}

// ---------- alignment（移植自 src/LFW/ui/alignment.ts 的解析部分） ----------

struct AlignOffset {
  double l = 0.0;
  double r = 0.0;
  double t = 0.0;
  double b = 0.0;
};

bool is_align_ws(char16_t c) {
  return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
}

char16_t lower_char(char16_t c) {
  return (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

bool is_side_char(char16_t c) { return c == u'l' || c == u'r' || c == u't' || c == u'b'; }

/** 严格匹配 `-?\d+(\.\d+)?`（与 TS 里命名形式的正则一致） */
bool scan_signed_number(const std::u16string& s, double& out) {
  size_t i = 0;
  bool neg = false;
  if (i < s.size() && s[i] == u'-') {
    neg = true;
    ++i;
  }
  size_t lead = 0;
  double value = 0.0;
  while (i < s.size() && s[i] >= u'0' && s[i] <= u'9') {
    value = value * 10.0 + static_cast<double>(s[i] - u'0');
    ++i;
    ++lead;
  }
  if (lead == 0) return false;
  if (i < s.size() && s[i] == u'.') {
    ++i;
    double frac = 0.1;
    size_t fdig = 0;
    while (i < s.size() && s[i] >= u'0' && s[i] <= u'9') {
      value += static_cast<double>(s[i] - u'0') * frac;
      frac *= 0.1;
      ++i;
      ++fdig;
    }
    if (fdig == 0) return false;
  }
  if (i != s.size()) return false;
  out = neg ? -value : value;
  return true;
}

Value offset_value(const AlignOffset& o) {
  auto obj = std::make_shared<Object>();
  obj->set(u"l", Value(o.l));
  obj->set(u"r", Value(o.r));
  obj->set(u"t", Value(o.t));
  obj->set(u"b", Value(o.b));
  return Value(obj);
}

std::optional<AlignOffset> parse_offset_str(const std::u16string& raw) {
  const std::u16string text = js_trim(raw);
  if (text.empty()) return std::nullopt;

  bool named = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char16_t c = lower_char(text[i]);
    if (!is_side_char(c)) continue;
    size_t j = i + 1;
    while (j < text.size() && is_align_ws(text[j])) ++j;
    if (j < text.size() && (text[j] == u':' || text[j] == u'=')) {
      named = true;
      break;
    }
  }

  AlignOffset out;
  bool any = false;
  if (named) {
    for (const std::u16string& part : split_char(text, u',')) {
      const std::u16string decl = js_trim(part);
      if (decl.size() < 3) continue;
      const char16_t key = lower_char(decl[0]);
      if (!is_side_char(key)) continue;
      size_t j = 1;
      while (j < decl.size() && is_align_ws(decl[j])) ++j;
      if (j >= decl.size() || (decl[j] != u':' && decl[j] != u'=')) continue;
      ++j;
      while (j < decl.size() && is_align_ws(decl[j])) ++j;
      double n = 0.0;
      if (!scan_signed_number(js_trim(decl.substr(j)), n)) continue;
      if (key == u'l') out.l = n;
      else if (key == u'r') out.r = n;
      else if (key == u't') out.t = n;
      else out.b = n;
      any = true;
    }
    if (!any) return std::nullopt;
    return out;
  }

  std::vector<double> nums;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && (is_align_ws(text[i]) || text[i] == u',')) ++i;
    const size_t start = i;
    while (i < text.size() && !is_align_ws(text[i]) && text[i] != u',') ++i;
    if (i == start) continue;
    const double n = string_to_number(text.substr(start, i - start));
    if (std::isnan(n)) return std::nullopt;
    nums.push_back(n);
  }
  if (nums.empty()) return std::nullopt;
  if (nums.size() == 1) return AlignOffset{nums[0], nums[0], nums[0], nums[0]};
  if (nums.size() == 2) return AlignOffset{nums[0], nums[0], nums[1], nums[1]};
  return AlignOffset{nums[0], nums[1], nums[2], nums[3]};
}

/** `followed` 的值：单名（"parent" / "id:foo"）或四边对象（"l:a,r:b"） */
std::optional<Value> parse_followed_str(const std::u16string& raw) {
  const std::u16string text = js_trim(raw);
  if (text.empty()) return std::nullopt;

  bool pairs = text.find(u',') != std::u16string::npos;
  if (!pairs) {
    const char16_t c0 = lower_char(text[0]);
    size_t j = 1;
    while (j < text.size() && is_align_ws(text[j])) ++j;
    pairs = is_side_char(c0) && j < text.size() && (text[j] == u':' || text[j] == u'=');
  }
  if (!pairs) return Value(text);

  auto obj = std::make_shared<Object>();
  bool any = false;
  for (const std::u16string& part : split_char(text, u',')) {
    const std::u16string decl = js_trim(part);
    if (decl.size() < 3) continue;
    const char16_t key = lower_char(decl[0]);
    if (!is_side_char(key)) continue;
    size_t j = 1;
    while (j < decl.size() && is_align_ws(decl[j])) ++j;
    if (j >= decl.size() || (decl[j] != u':' && decl[j] != u'=')) continue;
    const std::u16string value = js_trim(decl.substr(j + 1));
    if (value.empty()) continue;
    obj->set(std::u16string(1, key), Value(value));
    any = true;
  }
  if (!any) return Value(text);
  return Value(obj);
}

bool nullish_ptr(const Value* v) {
  return v == nullptr || std::holds_alternative<std::monostate>(*v) || std::holds_alternative<NullTag>(*v);
}

double num_field(const Object& obj, const char16_t* key) {
  const Value* v = obj.get(key);
  return v != nullptr ? to_number(*v) : to_number(Value());
}

std::optional<Value> normalize_alignment(const Value& raw) {
  const Object* src = as_object(raw);
  if (src == nullptr) return std::nullopt;
  auto ret = std::make_shared<Object>();
  bool any = false;
  const auto side = [&](const char16_t* key) {
    const Value* v = src->get(key);
    if (v == nullptr) return;
    const std::u16string* s = std::get_if<std::u16string>(v);
    if (s == nullptr) return;
    const std::u16string lowered = to_lower_case(js_trim(*s));
    ret->set(key, Value(lowered));
    if (!lowered.empty()) any = true;
  };
  side(u"left");
  side(u"right");
  side(u"top");
  side(u"bottom");

  if (const Value* fv = src->get(u"followed"); fv != nullptr) {
    if (const std::u16string* s = std::get_if<std::u16string>(fv)) {
      ret->set(u"followed", Value(*s));
    } else if (const Object* fo = as_object(*fv)) {
      auto obj = std::make_shared<Object>();
      bool any_f = false;
      for (const char16_t* key : {u"l", u"r", u"t", u"b"}) {
        const Value* v = fo->get(key);
        if (v == nullptr) continue;
        if (const std::u16string* side_str = std::get_if<std::u16string>(v)) {
          obj->set(key, Value(*side_str));
          any_f = true;
        }
      }
      if (any_f) ret->set(u"followed", Value(obj));
    }
  }

  if (const Value* ov = src->get(u"offset"); ov != nullptr && as_object(*ov) != nullptr) {
    AlignOffset o{num_field(*as_object(*ov), u"l"), num_field(*as_object(*ov), u"r"),
                  num_field(*as_object(*ov), u"t"), num_field(*as_object(*ov), u"b")};
    if (std::isnan(o.l)) o.l = 0.0;
    if (std::isnan(o.r)) o.r = 0.0;
    if (std::isnan(o.t)) o.t = 0.0;
    if (std::isnan(o.b)) o.b = 0.0;
    ret->set(u"offset", offset_value(o));
  }

  if (const Value* ov = src->get(u"once"); ov != nullptr) {
    bool once = false;
    if (const bool* b = std::get_if<bool>(ov)) once = *b;
    else if (const double* d = std::get_if<double>(ov)) once = (*d == 1.0);
    else if (const std::u16string* s = std::get_if<std::u16string>(ov)) {
      once = (*s == u"1" || *s == u"true");
    }
    if (once) ret->set(u"once", Value(true));
  }

  if (!any) return std::nullopt;
  return Value(ret);
}

std::optional<Value> parse_alignment_object(const Value& value) {
  const Object* src = as_object(value);
  if (src == nullptr) return std::nullopt;
  auto raw = std::make_shared<Object>();
  for (const std::u16string& key : src->keys()) {
    if (key == u"offset") continue;
    raw->set(key, *src->get(key));
  }

  const Value* off = src->get(u"offset");
  bool has_offset = false;
  if (off != nullptr && std::holds_alternative<std::u16string>(*off)) {
    if (const std::optional<AlignOffset> parsed = parse_offset_str(std::get<std::u16string>(*off))) {
      raw->set(u"offset", offset_value(*parsed));
      has_offset = true;
    }
  } else if (off != nullptr && as_object(*off) != nullptr) {
    raw->set(u"offset", *off);
    has_offset = true;
  }

  if (!has_offset) {
    const double l = num_field(*src, u"offset_l");
    const double r = num_field(*src, u"offset_r");
    const double t = num_field(*src, u"offset_t");
    const double b = num_field(*src, u"offset_b");
    if (!std::isnan(l) || !std::isnan(r) || !std::isnan(t) || !std::isnan(b)) {
      raw->set(u"offset", offset_value(AlignOffset{std::isnan(l) ? 0.0 : l, std::isnan(r) ? 0.0 : r,
                                                   std::isnan(t) ? 0.0 : t, std::isnan(b) ? 0.0 : b}));
    }
  }

  if (nullish_ptr(src->get(u"followed"))) {
    const auto str_of = [&](const char16_t* key) -> std::optional<std::u16string> {
      const Value* v = src->get(key);
      if (v == nullptr) return std::nullopt;
      const std::u16string* s = std::get_if<std::u16string>(v);
      return s != nullptr ? std::optional<std::u16string>(*s) : std::nullopt;
    };
    const std::optional<std::u16string> l = str_of(u"followed_l");
    const std::optional<std::u16string> r = str_of(u"followed_r");
    const std::optional<std::u16string> t = str_of(u"followed_t");
    const std::optional<std::u16string> b = str_of(u"followed_b");
    if (l.has_value() || r.has_value() || t.has_value() || b.has_value()) {
      auto obj = std::make_shared<Object>();
      if (l.has_value()) obj->set(u"l", Value(*l));
      if (r.has_value()) obj->set(u"r", Value(*r));
      if (t.has_value()) obj->set(u"t", Value(*t));
      if (b.has_value()) obj->set(u"b", Value(*b));
      raw->set(u"followed", Value(obj));
    }
  }

  return normalize_alignment(Value(raw));
}

std::optional<Value> parse_alignment_str(const std::u16string& raw) {
  const std::u16string text = js_trim(raw);
  if (text.empty()) return std::nullopt;
  if (text[0] == u'{') {
    if (const std::optional<Value> json = json_parse(text)) return parse_alignment_object(*json);
    return std::nullopt;
  }

  auto info = std::make_shared<Object>();
  for (const std::u16string& part : split_char(text, u';')) {
    const std::u16string decl = js_trim(part);
    if (decl.empty()) continue;
    const size_t at = decl.find_first_of(u":=");
    if (at == std::u16string::npos) continue;
    const std::u16string key = to_lower_case(js_trim(decl.substr(0, at)));
    const std::u16string value = js_trim(decl.substr(at + 1));
    if (value.empty()) continue;
    if (key == u"left" || key == u"right" || key == u"top" || key == u"bottom") {
      info->set(key, Value(value));
    } else if (key == u"offset") {
      if (const std::optional<AlignOffset> parsed = parse_offset_str(value)) {
        info->set(u"offset", offset_value(*parsed));
      }
    } else if (key == u"followed") {
      if (const std::optional<Value> followed = parse_followed_str(value)) {
        info->set(u"followed", *followed);
      }
    } else if (key == u"once") {
      info->set(u"once", Value(to_lower_case(value) == u"true" || value == u"1"));
    }
  }
  return normalize_alignment(Value(info));
}

std::optional<Value> parse_alignment_attrs(const IXMLElement& el, const std::u16string& prefix,
                                           const char16_t* inline_name) {
  if (inline_name != nullptr) {
    const std::optional<std::u16string> inline_value = attr_of(el, inline_name);
    if (non_empty(inline_value)) return parse_alignment_str(*inline_value);
  }
  const auto get = [&](const char16_t* name) {
    std::u16string key = prefix;
    key += name;
    return attr_of(el, key.c_str());
  };
  auto info = std::make_shared<Object>();
  for (const char16_t* side : {u"left", u"right", u"top", u"bottom"}) {
    if (const std::optional<std::u16string> v = get(side); v.has_value()) {
      info->set(side, Value(*v));
    }
  }
  if (const std::optional<std::u16string> v = get(u"offset"); v.has_value()) {
    if (const std::optional<AlignOffset> parsed = parse_offset_str(*v)) {
      info->set(u"offset", offset_value(*parsed));
    }
  }
  const std::optional<std::u16string> followed = get(u"followed");
  if (followed.has_value()) info->set(u"followed", Value(*followed));
  const std::optional<std::u16string> followed_l = get(u"followed_l");
  const std::optional<std::u16string> followed_r = get(u"followed_r");
  const std::optional<std::u16string> followed_t = get(u"followed_t");
  const std::optional<std::u16string> followed_b = get(u"followed_b");
  if (followed_l.has_value() || followed_r.has_value() || followed_t.has_value() ||
      followed_b.has_value()) {
    auto obj = std::make_shared<Object>();
    const auto put = [&](const char16_t* key, const std::optional<std::u16string>& v) {
      if (v.has_value()) obj->set(key, Value(*v));
      else if (followed.has_value()) obj->set(key, Value(*followed));
    };
    put(u"l", followed_l);
    put(u"r", followed_r);
    put(u"t", followed_t);
    put(u"b", followed_b);
    info->set(u"followed", Value(obj));
  }
  if (const std::optional<std::u16string> v = get(u"once"); v.has_value()) {
    info->set(u"once", Value(to_lower_case(*v) == u"true" || *v == u"1"));
  }
  return normalize_alignment(Value(info));
}

std::optional<Value> parse_alignment_element(const IXMLElement& el) {
  for (const IXMLElement* c : el.children()) {
    if (c->tag() != u"properties") continue;
    if (const std::optional<Value> props = parse_alignment_object(c->as_object())) return props;
    break;
  }
  if (const std::optional<Value> attrs = parse_alignment_attrs(el, u"", u"value")) return attrs;
  return parse_alignment_str(el.text());
}

/** 多个 alignment 声明按文档顺序合并（后者覆盖前者；offset/followed 整体替换） */
Value merge_alignment(const std::vector<Value>& specs) {
  if (specs.size() == 1) return specs[0];
  auto out = std::make_shared<Object>();
  for (const Value& spec : specs) {
    const Object* src = as_object(spec);
    if (src == nullptr) continue;
    for (const char16_t* key : {u"left", u"right", u"top", u"bottom"}) {
      if (const Value* v = src->get(key)) out->set(key, *v);
    }
    if (const Value* v = src->get(u"offset")) out->set(u"offset", *v);
    if (const Value* v = src->get(u"followed")) out->set(u"followed", *v);
    if (const Value* v = src->get(u"once"); v != nullptr && truthy(*v)) {
      out->set(u"once", Value(true));
    }
  }
  return Value(out);
}

}  // namespace

Value xml_to_ui_info(const IXMLElement& el) {
  auto ret = std::make_shared<Object>();
  ret->set(u"id", str_or_undef(attr_of(el, u"id")));
  ret->set(u"name", str_or_undef(attr_of(el, u"name")));
  ret->set(u"i18n", str_or_undef(attr_of(el, u"i18n")));
  ret->set(u"color", str_or_undef(attr_of(el, u"color")));
  ret->set(u"background", str_or_undef(attr_of(el, u"background")));
  ret->set(u"foreground", str_or_undef(attr_of(el, u"foreground")));
  ret->set(u"outlineColor", str_or_undef(attr_of(el, u"outlineColor")));
  ret->set(u"template", str_or_undef(attr_of(el, u"template")));
  {
    const std::optional<std::u16string> af = attr_of(el, u"auto_focus");
    ret->set(u"auto_focus", (af.has_value() && *af == u"true") ? Value(true) : Value());
  }
  // flex / alignment：属性形式与多个子元素按文档顺序合并
  std::vector<Value> flex_specs;
  std::vector<Value> alignment_specs;
  if (const Value flex = parse_flex_attrs(el); truthy(flex)) flex_specs.push_back(flex);
  if (const std::optional<Value> alignment = parse_alignment_attrs(el, u"alignment_", u"alignment")) {
    alignment_specs.push_back(*alignment);
  }

  ret->set(u"pos", nums_or_undef(nums_attr_of(el, u"pos")));
  ret->set(u"size", nums_or_undef(nums_attr_of(el, u"size")));
  ret->set(u"center", nums_or_undef(nums_attr_of(el, u"center")));
  ret->set(u"scale", nums_or_undef(nums_attr_of(el, u"scale")));

  const auto set_num_attr = [&](const char16_t* name) {
    if (const std::optional<std::u16string> v = attr_of(el, name); v.has_value()) {
      ret->set(name, to_number(Value(*v)));
    }
  };
  const auto set_bool_attr = [&](const char16_t* name) {
    if (const std::optional<std::u16string> v = attr_of(el, name); v.has_value()) {
      ret->set(name, Value(*v == u"true"));
    }
  };
  set_num_attr(u"opacity");
  set_num_attr(u"count");
  set_bool_attr(u"visible");
  set_bool_attr(u"disabled");
  set_bool_attr(u"clips");
  set_bool_attr(u"local_only");
  set_bool_attr(u"ignore_esc");
  set_num_attr(u"backgroundAlpha");
  set_num_attr(u"foregroundAlpha");
  set_num_attr(u"outlineWidth");
  set_num_attr(u"outlineAlpha");

  std::vector<Value> items;
  std::vector<Value> components;
  auto templates = std::make_shared<Object>();
  std::shared_ptr<Object> values_obj;
  std::shared_ptr<Object> style_obj;
  bool style_any = false;

  for (const IXMLElement* child : el.children()) {
    const std::u16string& tag = child->tag();
    if (tag == u"attrs") continue;  // 已合并进本节点的属性（`attr_of`），不是子节点
    if (tag == u"node" || tag == u"item") {
      const std::optional<std::u16string> ref = child->attr(u"ref");
      items.push_back(non_empty(ref) ? Value(*ref) : xml_to_ui_info(*child));
    } else if (tag == u"values") {
      if (values_obj == nullptr) {
        values_obj = std::make_shared<Object>();
        ret->set(u"values", Value(values_obj));
      }
      const Value src = child->as_object();
      const Object* so = as_object(src);
      if (so != nullptr) {
        for (const std::u16string& k : so->keys()) values_obj->set(k, *so->get(k));
      }
    } else if (tag == u"actions") {
      auto actions = std::make_shared<Object>();
      for (const XmlAttr& a : child->attrs()) {
        if (a.value.find(u',') != std::u16string::npos) {
          actions->set(a.name, arr_of(split_trim(a.value)));
        } else {
          actions->set(a.name, Value(js_trim(a.value)));
        }
      }
      for (const IXMLElement* c : child->children()) {
        if (!is_action_place(c->tag())) continue;
        const Value val = parse_action_children(*c);
        const Value* prev = actions->get(c->tag());
        if (prev != nullptr) {
          auto merged = std::make_shared<Array>();
          if (const Array* pa = as_array(*prev)) {
            for (const Value& x : pa->items()) merged->push_back(x);
          } else {
            merged->push_back(*prev);
          }
          if (const Array* va = as_array(val)) {
            for (const Value& x : va->items()) merged->push_back(x);
          } else {
            merged->push_back(val);
          }
          actions->set(c->tag(), Value(merged));
        } else {
          actions->set(c->tag(), val);
        }
      }
      ret->set(u"actions", Value(actions));
    } else if (tag == u"components") {
      for (const IXMLElement* c : child->children()) components.push_back(parse_component(*c));
    } else if (tag == u"component") {
      components.push_back(parse_component(*child));
    } else if (tag == u"style") {
      // 多个 <style> 按文档顺序合并（后者覆盖前者）
      if (style_obj == nullptr) style_obj = std::make_shared<Object>();
      const Value style_v = child->as_object();
      if (const Object* src = as_object(style_v)) {
        for (const std::u16string& k : src->keys()) style_obj->set(k, *src->get(k));
        style_any = true;
      }
      const auto set_style_num = [&](const char16_t* name) {
        if (const std::optional<std::u16string> v = child->attr(name); v.has_value()) {
          style_obj->set(name, to_number(Value(*v)));
          style_any = true;
        }
      };
      set_style_num(u"line_width");
      set_style_num(u"scale");
      set_style_num(u"padding_l");
      set_style_num(u"padding_r");
      set_style_num(u"padding_t");
      set_style_num(u"padding_b");
      set_style_num(u"shadow_blur");
      set_style_num(u"shadow_offset_x");
      set_style_num(u"shadow_offset_y");
      set_style_num(u"underline_width");
      if (const std::optional<std::u16string> v = child->attr(u"smoothing"); v.has_value()) {
        style_obj->set(u"smoothing", Value(*v == u"true"));
        style_any = true;
      }
      if (const std::optional<std::u16string> v = child->attr(u"disposable"); v.has_value()) {
        style_obj->set(u"disposable", Value(*v == u"true"));
        style_any = true;
      }
    } else if (tag == u"img") {
      auto img = std::make_shared<Object>();
      const std::optional<std::u16string> path_attr = child->attr(u"path");
      img->set(u"path", non_empty(path_attr) ? Value(*path_attr) : str_or_undef(child->attr(u"src")));
      const auto num_attr_value = [&](const char16_t* name) -> Value {
        const std::optional<std::u16string> v = child->attr(name);
        return v.has_value() ? to_number(Value(*v)) : Value();
      };
      img->set(u"x", num_attr_value(u"x"));
      img->set(u"y", num_attr_value(u"y"));
      const Value w_v = num_attr_value(u"w");
      img->set(u"w", w_v);
      const Value h_v = num_attr_value(u"h");
      img->set(u"h", h_v);
      const Value dw_v = num_attr_value(u"dw");
      img->set(u"dw", truthy(dw_v) ? dw_v : w_v);
      const Value dh_v = num_attr_value(u"dh");
      img->set(u"dh", truthy(dh_v) ? dh_v : h_v);
      const char16_t* const num_keys[] = {
          u"flip_x",      u"flip_y",      u"wrapS",        u"wrapT",
          u"offsetX",     u"offsetY",     u"offsetAnimX",  u"offsetAnimY",
          u"offsetAnimR", u"repeatX",     u"repeatY",
      };
      for (const char16_t* name : num_keys) {
        if (const std::optional<std::u16string> v = child->attr(name); v.has_value()) {
          img->set(name, to_number(Value(*v)));
        }
      }
      if (const IXMLElement* np = child->child_by_tag(u"nine_patch"); np != nullptr) {
        auto np_obj = std::make_shared<Object>();
        const char16_t* const np_keys[] = {u"f_l", u"f_t", u"f_r", u"f_b", u"f_w",
                                           u"f_h", u"l_w", u"t_h", u"r_w", u"b_h"};
        for (const char16_t* k : np_keys) {
          const std::optional<std::u16string> v = np->attr(k);
          np_obj->set(k, v.has_value() ? to_number(Value(*v)) : Value());
        }
        img->set(u"nine_patch", Value(np_obj));
      }
      ret->set(u"img", Value(img));
    } else if (tag == u"template") {
      const std::optional<std::u16string> id = child->attr(u"id");
      std::u16string tid;
      if (non_empty(id)) {
        tid = *id;
      } else {
        const std::optional<std::u16string> nm = child->attr(u"name");
        if (non_empty(nm)) tid = *nm;
      }
      templates->set(tid, xml_to_ui_info(*child));
    } else if (tag == u"alignment") {
      if (const std::optional<Value> spec = parse_alignment_element(*child)) {
        alignment_specs.push_back(*spec);
      }
    } else if (tag == u"flex") {
      if (const Value spec = parse_flex_element(*child); truthy(spec)) flex_specs.push_back(spec);
    } else {
      auto comp = std::make_shared<Object>();
      comp->set(u"cls", Value(tag));
      const std::optional<std::u16string> args = child->attr(u"args");
      if (non_empty(args)) comp->set(u"args", arr_of(split_trim(*args)));
      components.push_back(Value(comp));
    }
  }

  if (!flex_specs.empty()) ret->set(u"flex", merge_flex(flex_specs));
  if (!alignment_specs.empty()) ret->set(u"alignment", merge_alignment(alignment_specs));
  if (style_any) ret->set(u"style", Value(style_obj));
  if (!items.empty()) {
    auto a = std::make_shared<Array>();
    for (const Value& v : items) a->push_back(v);
    ret->set(u"items", Value(a));
  }
  if (!components.empty()) {
    auto a = std::make_shared<Array>();
    for (const Value& v : components) a->push_back(v);
    ret->set(u"component", Value(a));
  }
  if (templates->size() != 0) ret->set(u"templates", Value(templates));

  return Value(ret);
}

}
