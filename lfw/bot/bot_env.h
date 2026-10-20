#pragma once

#include <memory>
#include <string>
#include <variant>

#include "lfw/controller/base_controller.h"
#include "lfw/core/same_ref.h"
#include "lfw/core/value.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace bot {


inline bool nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

inline Value or_nullish(const Value& v, const Value& fallback) {
  return nullish(v) ? fallback : v;
}

inline Value state_of_ref(const Value& ref) {
  return field_or(field_or(ref, u"frame"), u"state");
}

inline Value field_key(const Value& v, const std::u16string& key) {
  const Object* o = as_object(v);
  if (o == nullptr) return Value();
  const Value* p = o->get(key);
  return p != nullptr ? *p : Value();
}

inline Value state_of_entity(const Entity& e) { return field_or(e.frame, u"state"); }

inline Value base_type_of_ref(const Value& ref) {
  return or_nullish(
      field_or(field_or(field_or(ref, u"data"), u"base"), u"type"), Value(0.0));
}

inline Value bot_ignore_of_ref(const Value& ref) {
  return or_nullish(
      field_or(field_or(ref, u"frame"), u"bot_ignore"),
      field_or(field_or(field_or(ref, u"data"), u"base"), u"bot_ignore"));
}

inline bool same_entity(const Value& a, const Value& b) {
  if (nullish(a) || nullish(b)) return strict_equals(a, b);
  const Value ia = field_or(a, u"id");
  const Value ib = field_or(b, u"id");
  if (truthy(ia) && truthy(ib)) return strict_equals(ia, ib);
  return same_ref(a, b);
}

inline Value holding_base_type_of(const Entity& e) {
  if (e.holding == nullptr) return Value();
  return base_type_of_ref(ref_of(*e.holding));
}

inline Value self_ref_of(const Entity& e) {
  Object pos;
  pos.set(u"x", Value(e.position.x));
  pos.set(u"y", Value(e.position.y));
  pos.set(u"z", Value(e.position.z));
  Object o;
  o.set(u"id", Value(e.id));
  o.set(u"position", Value(std::make_shared<Object>(pos)));
  return Value(std::make_shared<Object>(o));
}

}
}
