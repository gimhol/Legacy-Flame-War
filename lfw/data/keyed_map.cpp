#include "lfw/data/keyed_map.h"

#include <utility>

namespace lfw {

const Value* KeyedMap::find(std::u16string_view key) const {
  const Object* const o = as_object(_raw);
  return o != nullptr ? o->get(key) : nullptr;
}

const Value& KeyedMap::get(std::u16string_view key) const {
  static const Value kUndefined;
  const Value* const v = find(key);
  return v != nullptr ? *v : kUndefined;
}

double KeyedMap::num(std::u16string_view key) const { return to_number(get(key)); }

std::vector<std::u16string> KeyedMap::keys() const {
  const Object* const o = as_object(_raw);
  return o != nullptr ? o->keys() : std::vector<std::u16string>();
}

}