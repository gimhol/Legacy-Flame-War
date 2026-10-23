#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

inline Value field_or(const Value& v, const char16_t* key) {
  const Object* o = as_object(v);
  if (o == nullptr) return Value();
  const Value* p = o->get(std::u16string_view(key));
  return p != nullptr ? *p : Value();
}

inline Value field_or(const Object& o, const char16_t* key) {
  const Value* p = o.get(std::u16string_view(key));
  return p != nullptr ? *p : Value();
}

inline Value field_or(const Object& o, const std::u16string& key) {
  const Value* p = o.get(key);
  return p != nullptr ? *p : Value();
}

inline Value field_or(const Value& v, std::u16string_view key) {
  const Object* o = as_object(v);
  if (o == nullptr) return Value();
  const Value* p = o->get(key);
  return p != nullptr ? *p : Value();
}

inline Value field_or(const Object& o, std::u16string_view key) {
  const Value* p = o.get(key);
  return p != nullptr ? *p : Value();
}

}
