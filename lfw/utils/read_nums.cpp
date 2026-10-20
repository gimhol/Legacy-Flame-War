#include "lfw/utils/read_nums.h"

#include <cmath>
#include <memory>
#include <string>
#include <variant>

#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/utils/type_check.h"

namespace lfw {

namespace {

bool is_number(const Value& v) { return std::holds_alternative<double>(v); }

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

bool is_num_arr(const Value& v) {
  const Array* a = as_array(v);
  if (a == nullptr) return false;
  for (size_t i = 0; i < a->size(); ++i) {
    const double* const d = std::get_if<double>(&a->at(i));
    if (d != nullptr && std::isnan(*d)) return false;
  }
  return true;
}

Value to_number_of_text(const std::u16string& s) {
  std::u16string t;
  for (const char16_t c : s) {
    if (!is_str_white_space(c)) t.push_back(c);
  }
  return Value(string_to_number(t));
}

Value as_array_of(const Value& v) {
  Array a;
  a.push_back(v);
  return Value(std::make_shared<Array>(a));
}

}

bool read_nums(const Value& src, double len, const Value& fallbacks_in, std::vector<Value>& out,
               std::u16string* error) {
  const auto fail = [&](const char16_t* tail, const Value& got) {
    if (error != nullptr) {
      *error = u"[read_nums] failed, " + std::u16string(tail) + u", but got " + to_string(got);
    }
    return false;
  };
  out.clear();
  Value fallbacks = fallbacks_in;
  if (is_undefined(fallbacks)) fallbacks = Value(std::make_shared<Array>(Array()));
  if (is_number(fallbacks)) fallbacks = as_array_of(fallbacks);
  if (!is_num_arr(fallbacks)) return fail(u"fallbacks must be number[]", fallbacks);
  if (len < 1) return true;

  Array* const fb = as_array(fallbacks);
  while (fb->size() < len) {
    const Value last = fb->empty() ? Value() : fb->at(fb->size() - 1);
    fb->push_back(truthy(last) ? last : Value(0.0));
  }
  if (!truthy(src)) {
    out = fb->items();
    return true;
  }

  Value values = src;
  if (const std::u16string* text = std::get_if<std::u16string>(&values)) {
    Array a;
    const std::u16string& s = *text;
    size_t start = 0;
    for (;;) {
      const size_t comma = s.find(u',', start);
      const std::u16string piece =
          comma == std::u16string::npos ? s.substr(start) : s.substr(start, comma - start);
      a.push_back(to_number_of_text(piece));
      if (comma == std::u16string::npos) break;
      start = comma + 1;
    }
    values = Value(std::make_shared<Array>(a));
  }
  if (!is_num_arr(values)) return fail(u"src must be string or number[]", values);
  const Array& list = *as_array(values);

  for (size_t idx = 0; idx < len; ++idx) {
    if (idx > list.size()) out.push_back(fb->at(idx));
    else out.push_back(idx < list.size() ? list.at(idx) : Value());
  }
  return true;
}

}
