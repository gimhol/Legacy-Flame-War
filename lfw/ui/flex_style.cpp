#include "lfw/ui/flex_style.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "lfw/utils/string_help.h"

namespace lfw::ui {

namespace {

using Table = std::initializer_list<std::pair<const char16_t*, const char16_t*>>;

std::u16string lower_key(const std::u16string& s) {
  std::u16string out = js_trim(s);
  for (char16_t& c : out) {
    if (c == u'-') {
      c = u'_';
    } else if (c >= u'A' && c <= u'Z') {
      c = static_cast<char16_t>(c - u'A' + u'a');
    }
  }
  return out;
}

std::u16string alias_of(const std::u16string& key) {
  static const std::map<std::u16string, std::u16string> kAlias{
      {u"flex_direction", u"direction"},     {u"flex_wrap", u"wrap"},
      {u"justify_content", u"justify"},      {u"align_items", u"align"},
      {u"flex_align_self", u"align_self"},   {u"flex_grow", u"grow"},
      {u"flex_shrink", u"shrink"},           {u"flex_basis", u"basis"},
      {u"flex_order", u"order"},             {u"column_gap", u"col_gap"},
  };
  const std::map<std::u16string, std::u16string>::const_iterator it = kAlias.find(key);
  return it == kAlias.end() ? key : it->second;
}

std::u16string norm(const std::u16string& value, Table table, const char16_t* fallback) {
  const std::u16string key = lower_key(value);
  for (const std::pair<const char16_t*, const char16_t*>& kv : table) {
    if (key == kv.first) return std::u16string(kv.second);
  }
  return std::u16string(fallback);
}

std::u16string norm_of(const Value& v, Table table, const char16_t* fallback) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  if (s == nullptr) return std::u16string(fallback);
  return norm(*s, table, fallback);
}

double num_or(const Value& v, double fallback) {
  if (std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v)) {
    return fallback;
  }
  if (const std::u16string* const s = std::get_if<std::u16string>(&v); s != nullptr && s->empty()) {
    return fallback;
  }
  const double n = to_number(v);
  return std::isnan(n) ? fallback : n;
}

bool bool_or(const Value& v) {
  if (const bool* const b = std::get_if<bool>(&v)) return *b;
  if (const double* const d = std::get_if<double>(&v)) return *d != 0.0;
  if (const std::u16string* const s = std::get_if<std::u16string>(&v)) {
    return *s == u"true" || *s == u"1";
  }
  return false;
}

std::vector<double> padding_nums(const Value& v) {
  std::vector<double> out;
  if (std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v)) return out;
  if (const double* const d = std::get_if<double>(&v)) {
    out.push_back(*d);
    return out;
  }
  if (const Array* const a = as_array(v)) {
    for (size_t i = 0; i < a->size(); ++i) {
      const double n = to_number(a->at(i));
      if (!std::isnan(n)) out.push_back(n);
    }
    return out;
  }
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  if (s == nullptr) return out;
  std::u16string current;
  for (size_t i = 0; i <= s->size(); ++i) {
    const char16_t c = i < s->size() ? (*s)[i] : u' ';
    if (c == u',' || c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || i == s->size()) {
      if (!current.empty()) {
        const double n = to_number(Value(current));
        if (!std::isnan(n)) out.push_back(n);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }
  return out;
}

FlexPadding padding_of(const Value& v) {
  const std::vector<double> nums = padding_nums(v);
  FlexPadding out;
  switch (nums.size()) {
    case 0:
      break;
    case 1:
      out.top = out.right = out.bottom = out.left = nums[0];
      break;
    case 2:
      out.top = out.bottom = nums[0];
      out.right = out.left = nums[1];
      break;
    case 3:
      out.top = nums[0];
      out.right = out.left = nums[1];
      out.bottom = nums[2];
      break;
    default:
      out.top = nums[0];
      out.right = nums[1];
      out.bottom = nums[2];
      out.left = nums[3];
      break;
  }
  return out;
}

}  // namespace

void FlexStyle::assign(const Value& data) {
  std::map<std::u16string, Value> raw;
  if (const std::u16string* const text = std::get_if<std::u16string>(&data)) {
    size_t start = 0;
    while (start <= text->size()) {
      const size_t end = text->find(u';', start);
      const std::u16string part =
          text->substr(start, end == std::u16string::npos ? std::u16string::npos : end - start);
      start = end == std::u16string::npos ? text->size() + 1 : end + 1;
      const size_t colon = part.find(u':');
      if (colon == std::u16string::npos) continue;
      const std::u16string key = alias_of(lower_key(part.substr(0, colon)));
      if (key.empty()) continue;
      raw[key] = Value(js_trim(part.substr(colon + 1)));
    }
  } else if (const Object* const o = as_object(data)) {
    for (const std::u16string& k : o->keys()) {
      const Value* const v = o->get(k);
      if (v != nullptr) raw[alias_of(lower_key(k))] = *v;
    }
  } else {
    return;
  }
  if (raw.empty()) return;

  const auto get = [&raw](const char16_t* key) -> const Value* {
    const std::map<std::u16string, Value>::const_iterator it = raw.find(std::u16string(key));
    return it == raw.end() ? nullptr : &it->second;
  };

  static const Table kDisplay{{u"block", u"block"}, {u"flex", u"flex"}};
  static const Table kDirection{{u"row", u"row"},
                                {u"row_reverse", u"row-reverse"},
                                {u"column", u"column"},
                                {u"column_reverse", u"column-reverse"}};
  static const Table kWrap{
      {u"nowrap", u"nowrap"}, {u"wrap", u"wrap"}, {u"wrap_reverse", u"wrap-reverse"}};
  static const Table kAlign{{u"start", u"flex-start"},   {u"flex_start", u"flex-start"},
                            {u"left", u"flex-start"},    {u"end", u"flex-end"},
                            {u"flex_end", u"flex-end"},  {u"right", u"flex-end"},
                            {u"center", u"center"},      {u"stretch", u"stretch"}};
  static const Table kAlignSelf{{u"auto", u"auto"},      {u"start", u"flex-start"},
                                {u"flex_start", u"flex-start"}, {u"left", u"flex-start"},
                                {u"end", u"flex-end"},   {u"flex_end", u"flex-end"},
                                {u"right", u"flex-end"}, {u"center", u"center"},
                                {u"stretch", u"stretch"}};
  static const Table kJustify{{u"start", u"flex-start"},
                              {u"flex_start", u"flex-start"},
                              {u"left", u"flex-start"},
                              {u"end", u"flex-end"},
                              {u"flex_end", u"flex-end"},
                              {u"right", u"flex-end"},
                              {u"center", u"center"},
                              {u"between", u"space-between"},
                              {u"space_between", u"space-between"},
                              {u"around", u"space-around"},
                              {u"space_around", u"space-around"},
                              {u"evenly", u"space-evenly"},
                              {u"space_evenly", u"space-evenly"}};
  static const Table kAlignContent{{u"start", u"flex-start"},
                                   {u"flex_start", u"flex-start"},
                                   {u"end", u"flex-end"},
                                   {u"flex_end", u"flex-end"},
                                   {u"center", u"center"},
                                   {u"stretch", u"stretch"},
                                   {u"between", u"space-between"},
                                   {u"space_between", u"space-between"},
                                   {u"around", u"space-around"},
                                   {u"space_around", u"space-around"}};

  const Value* const display = get(u"display");
  _display = display != nullptr ? norm_of(*display, kDisplay, u"block") : std::u16string(u"block");

  const Value* const direction = get(u"direction");
  _direction =
      direction != nullptr ? norm_of(*direction, kDirection, u"row") : std::u16string(u"row");

  const Value* const wrap = get(u"wrap");
  _wrap = wrap != nullptr ? norm_of(*wrap, kWrap, u"nowrap") : std::u16string(u"nowrap");

  const Value* const justify = get(u"justify");
  _justify = justify != nullptr ? norm_of(*justify, kJustify, u"flex-start")
                                : std::u16string(u"flex-start");

  const Value* const align = get(u"align");
  _align = align != nullptr ? norm_of(*align, kAlign, u"stretch") : std::u16string(u"stretch");

  const Value* const align_content = get(u"align_content");
  _align_content = align_content != nullptr ? norm_of(*align_content, kAlignContent, u"stretch")
                                            : std::u16string(u"stretch");

  const Value* const align_self = get(u"align_self");
  _align_self =
      align_self != nullptr ? norm_of(*align_self, kAlignSelf, u"auto") : std::u16string(u"auto");

  const Value* const gap = get(u"gap");
  _gap = gap != nullptr ? num_or(*gap, 0.0) : 0.0;
  const Value* const row_gap = get(u"row_gap");
  _row_gap = row_gap != nullptr ? num_or(*row_gap, _gap) : _gap;
  const Value* const col_gap = get(u"col_gap");
  _col_gap = col_gap != nullptr ? num_or(*col_gap, _gap) : _gap;

  const Value* const padding = get(u"padding");
  _padding = padding != nullptr ? padding_of(*padding) : FlexPadding{};

  const Value* const grow = get(u"grow");
  _grow = grow != nullptr ? std::max(0.0, num_or(*grow, 0.0)) : 0.0;
  const Value* const shrink = get(u"shrink");
  _shrink = shrink != nullptr ? std::max(0.0, num_or(*shrink, 1.0)) : 1.0;

  const Value* const basis = get(u"basis");
  if (basis == nullptr || std::holds_alternative<std::monostate>(*basis) ||
      std::holds_alternative<NullTag>(*basis)) {
    _basis_auto = true;
    _basis = 0.0;
  } else {
    const std::u16string* const s = std::get_if<std::u16string>(basis);
    if (s != nullptr && (s->empty() || *s == u"auto")) {
      _basis_auto = true;
      _basis = 0.0;
    } else {
      _basis_auto = false;
      _basis = std::max(0.0, num_or(*basis, 0.0));
    }
  }

  const Value* const order = get(u"order");
  _order = order != nullptr ? num_or(*order, 0.0) : 0.0;

  const Value* const fit = get(u"fit");
  const bool fit_all = fit != nullptr && bool_or(*fit);
  const Value* const fit_w = get(u"fit_w");
  const Value* const fit_h = get(u"fit_h");
  _fit_w = fit_w != nullptr ? bool_or(*fit_w) : fit_all;
  _fit_h = fit_h != nullptr ? bool_or(*fit_h) : fit_all;

  if (on_change) on_change();
}

void FlexStyle::set_display(const std::u16string& v) {
  static const Table kDisplay{{u"block", u"block"}, {u"flex", u"flex"}};
  const std::u16string n = norm(v, kDisplay, u"block");
  if (n == _display) return;
  _display = n;
  if (on_change) on_change();
}

void FlexStyle::set_direction(const std::u16string& v) {
  static const Table kDirection{{u"row", u"row"},
                                {u"row_reverse", u"row-reverse"},
                                {u"column", u"column"},
                                {u"column_reverse", u"column-reverse"}};
  const std::u16string n = norm(v, kDirection, u"row");
  if (n == _direction) return;
  _direction = n;
  if (on_change) on_change();
}

void FlexStyle::set_align(const std::u16string& v) {
  static const Table kAlign{{u"start", u"flex-start"},   {u"flex_start", u"flex-start"},
                            {u"left", u"flex-start"},    {u"end", u"flex-end"},
                            {u"flex_end", u"flex-end"},  {u"right", u"flex-end"},
                            {u"center", u"center"},      {u"stretch", u"stretch"}};
  const std::u16string n = norm(v, kAlign, u"stretch");
  if (n == _align) return;
  _align = n;
  if (on_change) on_change();
}

void FlexStyle::set_row_gap(double v) {
  if (v == _row_gap) return;
  _row_gap = v;
  if (on_change) on_change();
}

void FlexStyle::set_col_gap(double v) {
  if (v == _col_gap) return;
  _col_gap = v;
  if (on_change) on_change();
}

void FlexStyle::set_shrink(double v) {
  const double n = std::max(0.0, v);
  if (n == _shrink) return;
  _shrink = n;
  if (on_change) on_change();
}

}  // namespace lfw::ui
