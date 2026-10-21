#include "lfw/data/hit_key_map.h"

#include <utility>

namespace lfw {
namespace {

const Value& put(Object* const o, Value& slot, const char16_t* key) {
  const Value* const v = o != nullptr ? o->get(key) : nullptr;
  if (v != nullptr) slot = *v;
  return slot;
}

}

HitKeyMap::HitKeyMap(Value raw) : _raw(std::move(raw)) {
  Object* const o = as_object(_raw);
  if (o == nullptr) return;
  put(o, _a, u"a");
  put(o, _j, u"j");
  put(o, _d, u"d");
  put(o, _U, u"U");
  put(o, _D, u"D");
  put(o, _L, u"L");
  put(o, _R, u"R");
  put(o, _F, u"F");
  put(o, _B, u"B");
  put(o, _aa, u"aa");
  put(o, _jj, u"jj");
  put(o, _dd, u"dd");
  put(o, _FF, u"FF");
  put(o, _BB, u"BB");
  put(o, _UU, u"UU");
  put(o, _DD, u"DD");
}

const Value* HitKeyMap::find(const std::u16string& key) const {
  const Object* const o = as_object(_raw);
  return o != nullptr ? o->get(key) : nullptr;
}

}