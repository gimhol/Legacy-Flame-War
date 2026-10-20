#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {


inline std::optional<std::u16string> get_str_or(const IXMLElement& el, const std::u16string& name,
                                                const Value& or_value) {
  std::optional<std::u16string> v = el.get_str(name);
  if (v) return v;
  if (const std::u16string* s = std::get_if<std::u16string>(&or_value)) return *s;
  return std::nullopt;
}

inline std::optional<double> get_num_or(const IXMLElement& el, const std::u16string& name,
                                        const Value& or_value) {
  std::optional<double> v = el.get_num(name);
  if (v) return v;
  if (const double* d = std::get_if<double>(&or_value)) return *d;
  return std::nullopt;
}

inline std::optional<bool> get_bool_or(const IXMLElement& el, const std::u16string& name,
                                       const Value& or_value) {
  std::optional<bool> v = el.get_bool(name);
  if (v) return v;
  if (const bool* b = std::get_if<bool>(&or_value)) return *b;
  return std::nullopt;
}

inline std::optional<std::u16string> opt_str(const Value& v) {
  if (const std::u16string* s = std::get_if<std::u16string>(&v)) return *s;
  return std::nullopt;
}

inline std::optional<double> opt_num(const Value& v) {
  if (const double* d = std::get_if<double>(&v)) return *d;
  return std::nullopt;
}

inline std::optional<bool> opt_bool(const Value& v) {
  if (const bool* b = std::get_if<bool>(&v)) return *b;
  return std::nullopt;
}

inline Value from_opt(const std::optional<std::u16string>& v) {
  return v ? Value(*v) : Value();
}

inline Value from_opt(const std::optional<double>& v) {
  return v ? Value(*v) : Value();
}

inline Value from_opt(const std::optional<bool>& v) {
  return v ? Value(*v) : Value();
}

inline Value from_opt(const std::optional<std::vector<std::u16string>>& v) {
  if (!v) return Value();
  auto arr = std::make_shared<Array>();
  for (const std::u16string& s : *v) arr->push_back(Value(s));
  return Value(std::move(arr));
}

inline Value from_opt(const std::optional<std::vector<double>>& v) {
  if (!v) return Value();
  auto arr = std::make_shared<Array>();
  for (double d : *v) arr->push_back(Value(d));
  return Value(std::move(arr));
}

inline Value field_join(const Value& v) {
  if (std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v)) {
    return Value();
  }
  const Array* const a = as_array(v);
  if (a == nullptr) return v;
  std::u16string out;
  for (size_t i = 0; i < a->size(); ++i) {
    if (i != 0) out.push_back(u',');
    const Value& item = a->at(i);
    if (std::holds_alternative<std::monostate>(item) ||
        std::holds_alternative<NullTag>(item)) {
      continue;
    }
    out += to_string(item);
  }
  return Value(std::move(out));
}

}
}
}
