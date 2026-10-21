#pragma once

#include <memory>
#include <string>
#include <variant>

#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace bot {


inline bool nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

inline Value or_nullish(const Value& v, const Value& fallback) {
  return nullish(v) ? fallback : v;
}

inline Value field_key(const Value& v, const std::u16string& key) {
  const Object* o = as_object(v);
  if (o == nullptr) return Value();
  const Value* p = o->get(key);
  return p != nullptr ? *p : Value();
}

inline Value state_of_entity(const Entity& e) { return field_or(e.frame, u"state"); }

inline Value base_type_of(const Entity& e) { return Value(e.base_type()); }

inline Value bot_ignore_of(const Entity& e) {
  return or_nullish(field_or(e.frame, u"bot_ignore"), field_or(e.data().base(), u"bot_ignore"));
}

inline Value holding_base_type_of(const Entity& e) {
  if (e.holding == nullptr) return Value();
  return base_type_of(*e.holding);
}

}
}
