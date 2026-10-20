#pragma once

#include <cmath>
#include <variant>

#include "lfw/core/js_string.h"
#include "lfw/core/value.h"

namespace lfw {

inline bool is_nan_num(const Value& v) {
  const double* d = std::get_if<double>(&v);
  return d != nullptr && std::isnan(*d);
}

inline bool is_num(double d) { return !std::isnan(d) && std::isfinite(d); }

inline bool is_num(const Value& v) {
  const double* d = std::get_if<double>(&v);
  return d != nullptr && is_num(*d);
}

inline bool is_positive(const Value& v) {
  const double* d = std::get_if<double>(&v);
  return d != nullptr && is_num(*d) && *d > 0;
}

inline bool not_zero_num(const Value& v) {
  const double* d = std::get_if<double>(&v);
  return d != nullptr && is_num(*d) && *d != 0;
}

inline bool is_int(double d) { return std::isfinite(d) && std::trunc(d) == d; }

inline bool is_int(const Value& v) {
  const double* d = std::get_if<double>(&v);
  return d != nullptr && is_int(*d);
}

inline bool is_positive_int(const Value& v) { return is_int(v) && to_number(v) > 0; }

inline bool is_non_nagative_int(const Value& v) { return is_int(v) && to_number(v) >= 0; }

inline bool is_undefined_or_number(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v) ||
         is_num(v);
}

inline bool is_str(const Value& v) { return std::holds_alternative<std::u16string>(v); }

inline bool is_non_empty_str(const Value& v) { return truthy(v) && is_str(v); }

inline bool is_non_blank_str(const Value& v) {
  const std::u16string* const text = std::get_if<std::u16string>(&v);
  if (text == nullptr) return false;
  for (const char16_t c : *text) {
    if (!is_str_white_space(c)) return true;
  }
  return false;
}

}
