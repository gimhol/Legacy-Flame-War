#include "lfw/data/frame_records.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace {

double num_field(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::nan("");
}

std::vector<double> nums(const Object& o, const char16_t* key) {
  std::vector<double> out;
  const Value* const v = o.get(key);
  const Array* const a = v != nullptr ? as_array(*v) : nullptr;
  if (a == nullptr) return out;
  out.reserve(a->size());
  for (const Value& x : a->items()) out.push_back(to_number(x));
  return out;
}

std::vector<std::u16string> texts(const Object& o, const char16_t* key) {
  std::vector<std::u16string> out;
  const Value* const v = o.get(key);
  const Array* const a = v != nullptr ? as_array(*v) : nullptr;
  if (a == nullptr) return out;
  out.reserve(a->size());
  for (const Value& x : a->items()) out.push_back(to_string(x));
  return out;
}

std::optional<bool> opt_bool(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const bool* const b = v != nullptr ? std::get_if<bool>(v) : nullptr;
  if (b == nullptr) return std::nullopt;
  return *b;
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


StringOrList::StringOrList(Value raw) : _raw(std::move(raw)) {
  if (const std::u16string* const s = std::get_if<std::u16string>(&_raw)) {
    _str = *s;
    _list.push_back(*s);
  } else if (const Array* const a = as_array(_raw)) {
    for (const Value& v : a->items()) _list.push_back(to_string(v));
  }
}

namespace {
const std::vector<std::u16string>& empty_texts() {
  static const std::vector<std::u16string> kEmpty;
  return kEmpty;
}
}

const std::vector<std::u16string>& StringOrList::items() const { return _list; }

FrameModel::FrameModel(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  {
    const Value* const v = o->get(u"id");
    const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
    if (s != nullptr) _id = *s;
  }
  {
    const Value* const v = o->get(u"anim");
    const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
    if (s != nullptr) _anim = *s;
  }
  _bones = texts(*o, u"bones");
  _pos = nums(*o, u"pos");
  _rot = nums(*o, u"rot");
  _scl = nums(*o, u"scl");
  _seek = num_field(*o, u"seek");
  _seek_end = num_field(*o, u"seek_end");
  _time_scale = num_field(*o, u"time_scale");
  _rad = num_field(*o, u"rad");
  _hold_last = opt_bool(*o, u"hold_last");
  _loop = opt_bool(*o, u"loop");
  _reverse = opt_bool(*o, u"reverse");
  _pose = field_or(*o, u"pose");
  _rotation = field_or(*o, u"rotation");
  _scale = field_or(*o, u"scale");
  _offset = field_or(*o, u"offset");
}

}
