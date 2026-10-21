#include "lfw/data/opoint_info.h"

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

Opoint::Opoint(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = text(*o, u"name");
  _kind = num(*o, u"kind");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _pos_type = num(*o, u"pos_type");
  _dvx = num(*o, u"dvx");
  _dvy = num(*o, u"dvy");
  _dvz = num(*o, u"dvz");
  _max_hp = num(*o, u"max_hp");
  _hp = num(*o, u"hp");
  _max_mp = num(*o, u"max_mp");
  _mp = num(*o, u"mp");
  _speedz = num(*o, u"speedz");
  _spreading = num(*o, u"spreading");
  _ghost = num(*o, u"ghost");
  _interval = num(*o, u"interval");
  _interval_mode = num(*o, u"interval_mode");
  _motionless = num(*o, u"motionless");
  _unimportant = num(*o, u"unimportant");
  _delay = num(*o, u"delay");
  _inherit_speed_x = num(*o, u"inherit_speed_x");
  _inherit_speed_y = num(*o, u"inherit_speed_y");
  _inherit_speed_z = num(*o, u"inherit_speed_z");
  _oid = raw_of(*o, u"oid");
  _action = raw_of(*o, u"action");
  _multi = raw_of(*o, u"multi");
  _interval_id = raw_of(*o, u"interval_id");
  _indicator_info = raw_of(*o, u"__indicator_info");
  _gen_x = text(*o, u"gen_x");
  _compiled_gen_x = raw_of(*o, u"__gen_x");
  _gen_y = text(*o, u"gen_y");
  _compiled_gen_y = raw_of(*o, u"__gen_y");
  _gen_z = text(*o, u"gen_z");
  _compiled_gen_z = raw_of(*o, u"__gen_z");
  _gen_dvx = text(*o, u"gen_dvx");
  _compiled_gen_dvx = raw_of(*o, u"__gen_dvx");
  _gen_dvy = text(*o, u"gen_dvy");
  _compiled_gen_dvy = raw_of(*o, u"__gen_dvy");
  _gen_dvz = text(*o, u"gen_dvz");
  _compiled_gen_dvz = raw_of(*o, u"__gen_dvz");
  _gen_spread_x = text(*o, u"gen_spread_x");
  _compiled_gen_spread_x = raw_of(*o, u"__gen_spread_x");
  _gen_spread_y = text(*o, u"gen_spread_y");
  _compiled_gen_spread_y = raw_of(*o, u"__gen_spread_y");
  _gen_spread_z = text(*o, u"gen_spread_z");
  _compiled_gen_spread_z = raw_of(*o, u"__gen_spread_z");
}

}