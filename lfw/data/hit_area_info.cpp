#include "lfw/data/hit_area_info.h"

#include <cmath>
#include <variant>
#include <utility>

namespace lfw {
namespace {

double num(const Object& o, const char16_t* key, std::uint32_t& has, std::uint32_t bit) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nan("");
  if (!std::holds_alternative<NullTag>(*v)) has |= bit;
  return to_number(*v);
}

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
  _x = num(*o, u"x", _has, kHasX);
  _y = num(*o, u"y", _has, kHasY);
  _w = num(*o, u"w", _has, kHasW);
  _h = num(*o, u"h", _has, kHasH);
  _z = num(*o, u"z", _has, kHasZ);
  _l = num(*o, u"l", _has, kHasL);
  _kind = num(*o, u"kind", _has, kHasKind);
  _hit_flag = num(*o, u"hit_flag", _has, kHasHitFlag);
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
  _x = num(*o, u"x", _has, kHasX);
  _y = num(*o, u"y", _has, kHasY);
  _w = num(*o, u"w", _has, kHasW);
  _h = num(*o, u"h", _has, kHasH);
  _z = num(*o, u"z", _has, kHasZ);
  _l = num(*o, u"l", _has, kHasL);
  _kind = num(*o, u"kind", _has, kHasKind);
  _hit_flag = num(*o, u"hit_flag", _has, kHasHitFlag);
  _kind_name = text(*o, u"kind_name");
  _hit_flag_name = text(*o, u"hit_flag_name");
  _motionless = num(*o, u"motionless", _has, kHasMotionless);
  _shaking = num(*o, u"shaking", _has, kHasShaking);
  _dvx = num(*o, u"dvx", _has, kHasDvx);
  _dvy = num(*o, u"dvy", _has, kHasDvy);
  _dvz = num(*o, u"dvz", _has, kHasDvz);
  _fall = num(*o, u"fall", _has, kHasFall);
  _vrest = num(*o, u"vrest", _has, kHasVrest);
  _arest = num(*o, u"arest", _has, kHasArest);
  _bdefend = num(*o, u"bdefend", _has, kHasBdefend);
  _injury = num(*o, u"injury", _has, kHasInjury);
  _effect = num(*o, u"effect", _has, kHasEffect);
  _effect_name = text(*o, u"effect_name");
  _prefab_id = text(*o, u"prefab_id");
  _test = text(*o, u"test");
  _interval = num(*o, u"interval", _has, kHasInterval);
  _interval_mode = num(*o, u"interval_mode", _has, kHasIntervalMode);
  _interval_id = raw_of(*o, u"interval_id");
  _actions = raw_of(*o, u"actions");
  _catchingact = raw_of(*o, u"catchingact");
  _caughtact = raw_of(*o, u"caughtact");
  _on_hit_ground = raw_of(*o, u"on_hit_ground");
  _tester = raw_of(*o, u"__tester");
  _indicator_info = raw_of(*o, u"__indicator_info");
}

}