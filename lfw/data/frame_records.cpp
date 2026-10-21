#include "lfw/data/frame_records.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

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

}

Cpoint::Cpoint(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _kind = num(*o, u"kind");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _injury = num(*o, u"injury");
  _hurtable = num(*o, u"hurtable");
  _decrease = num(*o, u"decrease");
  _throwvx = num(*o, u"throwvx");
  _throwvy = num(*o, u"throwvy");
  _throwvz = num(*o, u"throwvz");
  _throwinjury = num(*o, u"throwinjury");
  _shaking = num(*o, u"shaking");
  _motionless = num(*o, u"motionless");
  _fronthurtact = text(*o, u"fronthurtact");
  _backhurtact = text(*o, u"backhurtact");
  _vaction = field_or(*o, u"vaction");
  _indicator_info = field_or(*o, u"__indicator_info");
}

Wpoint::Wpoint(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _kind = num(*o, u"kind");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _dvx = num(*o, u"dvx");
  _dvy = num(*o, u"dvy");
  _dvz = num(*o, u"dvz");
  _weaponact = text(*o, u"weaponact");
  _attacking = text(*o, u"attacking");
  _indicator_info = field_or(*o, u"__indicator_info");
}

Bpoint::Bpoint(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _r = num(*o, u"r");
  _indicator_info = field_or(*o, u"__indicator_info");
}

ChaseInfo::ChaseInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _stratedy = num(*o, u"stratedy");
  _flag = num(*o, u"flag");
  _lost = num(*o, u"lost");
  _oy = num(*o, u"oy");
  _overshoot = field_or(*o, u"overshoot");
}

}