#include "lfw/data/hit_area_info.h"

#include <cmath>
#include <utility>

namespace lfw {
namespace {

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::nan("");
}

std::u16string text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

Value raw_of(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? *v : Value();
}

}

BdyInfo::BdyInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = text(*o, u"name");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _w = num(*o, u"w");
  _h = num(*o, u"h");
  _z = num(*o, u"z");
  _l = num(*o, u"l");
  _kind = num(*o, u"kind");
  _hit_flag = num(*o, u"hit_flag");
  _kind_name = text(*o, u"kind_name");
  _hit_flag_name = text(*o, u"hit_flag_name");
  _prefab_id = text(*o, u"prefab_id");
  _test = text(*o, u"test");
  _actions = raw_of(*o, u"actions");
  _on_hit_ground = raw_of(*o, u"on_hit_ground");
  _tester = raw_of(*o, u"__tester");
  _indicator_info = raw_of(*o, u"__indicator_info");
}

ItrInfo::ItrInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = text(*o, u"name");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _w = num(*o, u"w");
  _h = num(*o, u"h");
  _z = num(*o, u"z");
  _l = num(*o, u"l");
  _kind = num(*o, u"kind");
  _hit_flag = num(*o, u"hit_flag");
  _kind_name = text(*o, u"kind_name");
  _hit_flag_name = text(*o, u"hit_flag_name");
  _motionless = num(*o, u"motionless");
  _shaking = num(*o, u"shaking");
  _dvx = num(*o, u"dvx");
  _dvy = num(*o, u"dvy");
  _dvz = num(*o, u"dvz");
  _fall = num(*o, u"fall");
  _vrest = num(*o, u"vrest");
  _arest = num(*o, u"arest");
  _bdefend = num(*o, u"bdefend");
  _injury = num(*o, u"injury");
  _effect = num(*o, u"effect");
  _effect_name = text(*o, u"effect_name");
  _prefab_id = text(*o, u"prefab_id");
  _test = text(*o, u"test");
  _interval = num(*o, u"interval");
  _interval_mode = num(*o, u"interval_mode");
  _interval_id = raw_of(*o, u"interval_id");
  _actions = raw_of(*o, u"actions");
  _catchingact = raw_of(*o, u"catchingact");
  _caughtact = raw_of(*o, u"caughtact");
  _on_hit_ground = raw_of(*o, u"on_hit_ground");
  _tester = raw_of(*o, u"__tester");
  _indicator_info = raw_of(*o, u"__indicator_info");
}

}