#include "lfw/data/frame_info.h"

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/frame_id.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace {

double num_of(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::numeric_limits<double>::quiet_NaN();
}

std::u16string text_of(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

std::optional<bool> opt_bool(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const bool* const b = v != nullptr ? std::get_if<bool>(v) : nullptr;
  if (b == nullptr) return std::nullopt;
  return *b;
}

FrameInfo make_empty_frame() {
  Object pic;
  pic.set(u"tex", Value(std::u16string()));
  pic.set(u"x", Value(0.0));
  pic.set(u"y", Value(0.0));
  pic.set(u"w", Value(0.0));
  pic.set(u"h", Value(0.0));

  Object o;
  o.set(u"id", Value(std::u16string(frame_id::kNone)));
  o.set(u"name", Value(std::u16string(u"EMPTY_FRAME_INFO")));
  o.set(u"pic", Value(std::make_shared<Object>(pic)));
  o.set(u"width", Value(0.0));
  o.set(u"height", Value(0.0));
  o.set(u"state", Value(std::nan("")));
  o.set(u"wait", Value(0.0));
  o.set(u"centerx", Value(0.0));
  o.set(u"centery", Value(0.0));
  const Value* nf = defines::find(u"Defines.NEXT_FRAME_AUTO");
  if (nf != nullptr) o.set(u"next", *nf);
  return FrameInfo(Value(std::make_shared<Object>(o)));
}

FrameInfo make_gone_frame() {
  Object pic;
  pic.set(u"tex", Value(std::u16string(u"")));
  pic.set(u"x", Value(0.0));
  pic.set(u"y", Value(0.0));
  pic.set(u"w", Value(0.0));
  pic.set(u"h", Value(0.0));
  Object next;
  next.set(u"id", Value(std::u16string(frame_id::kGone)));
  Object o;
  o.set(u"id", Value(std::u16string(frame_id::kGone)));
  o.set(u"name", Value(std::u16string(u"GONE_FRAME_INFO")));
  o.set(u"pic", Value(std::make_shared<Object>(pic)));
  o.set(u"width", Value(0.0));
  o.set(u"height", Value(0.0));
  o.set(u"state", Value(std::nan("")));
  o.set(u"wait", Value(0.0));
  o.set(u"next", Value(std::make_shared<Object>(next)));
  o.set(u"centerx", Value(0.0));
  o.set(u"centery", Value(0.0));
  o.set(u"no_shadow", Value(1.0));
  return FrameInfo(Value(std::make_shared<Object>(o)));
}

}

FrameInfo::FrameInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text_of(*o, u"id");
  _name = text_of(*o, u"name");
  _state_name = text_of(*o, u"state_name");
  _state = num_of(*o, u"state");
  _wait = num_of(*o, u"wait");
  _centerx = num_of(*o, u"centerx");
  _centery = num_of(*o, u"centery");
  _width = num_of(*o, u"width");
  _height = num_of(*o, u"height");
  _hp = num_of(*o, u"hp");
  _mp = num_of(*o, u"mp");
  _invisible = num_of(*o, u"invisible");
  _invulnerable = num_of(*o, u"invulnerable");
  _blinking = num_of(*o, u"blinking");
  _bot_ignore = num_of(*o, u"bot_ignore");
  _no_shadow = num_of(*o, u"no_shadow");
  _jump_flag = num_of(*o, u"jump_flag");
  _behavior = num_of(*o, u"behavior");
  _facing = num_of(*o, u"facing");
  _landable = num_of(*o, u"landable");
  _stat_recover = num_of(*o, u"stat_recover");
  _toughness_recover = num_of(*o, u"toughness_recover");
  _dvx = num_of(*o, u"dvx");
  _dvy = num_of(*o, u"dvy");
  _dvz = num_of(*o, u"dvz");
  _vxm = num_of(*o, u"vxm");
  _vym = num_of(*o, u"vym");
  _vzm = num_of(*o, u"vzm");
  _acc_x = num_of(*o, u"acc_x");
  _acc_y = num_of(*o, u"acc_y");
  _acc_z = num_of(*o, u"acc_z");
  _ctrl_x = num_of(*o, u"ctrl_x");
  _ctrl_y = num_of(*o, u"ctrl_y");
  _ctrl_z = num_of(*o, u"ctrl_z");
  _aabb_x1 = num_of(*o, u"__aabb_x1");
  _aabb_x2 = num_of(*o, u"__aabb_x2");
  _aabb_z1 = num_of(*o, u"__aabb_z1");
  _aabb_z2 = num_of(*o, u"__aabb_z2");
  _gravity_enabled = opt_bool(*o, u"gravity_enabled");
  _ref = field_or(*o, u"ref");
  _pic = field_or(*o, u"pic");
  _pics = field_or(*o, u"pics");
  _model = field_or(*o, u"model");
  _next = field_or(*o, u"next");
  _sound = field_or(*o, u"sound");
  _hold = field_or(*o, u"hold");
  _hit = field_or(*o, u"hit");
  _key_down = field_or(*o, u"key_down");
  _key_up = field_or(*o, u"key_up");
  _seqs = field_or(*o, u"seqs");
  _bdy = field_or(*o, u"bdy");
  _itr = field_or(*o, u"itr");
  _wpoint = field_or(*o, u"wpoint");
  _bpoint = field_or(*o, u"bpoint");
  _opoint = field_or(*o, u"opoint");
  _cpoint = field_or(*o, u"cpoint");
  _chase = field_or(*o, u"chase");
  _broadcasts = field_or(*o, u"broadcasts");
  _dataset = field_or(*o, u"dataset");
  _on_dead = field_or(*o, u"on_dead");
  _on_exhaustion = field_or(*o, u"on_exhaustion");
  _on_landing = field_or(*o, u"on_landing");
  _on_restrict = field_or(*o, u"on_restrict");
  _on_x_restrict = field_or(*o, u"on_x_restrict");
  _on_y_restrict = field_or(*o, u"on_y_restrict");
  _on_z_restrict = field_or(*o, u"on_z_restrict");
  _indicator_info = field_or(*o, u"__indicator_info");
  _seq_map = field_or(*o, u"__seq_map");
  _hit_ground_itrs = field_or(*o, u"__hit_ground_itrs");
  _hit_ground_bdys = field_or(*o, u"__hit_ground_bdys");
  _tex = field_or(*o, u"__tex");
}

bool FrameInfo::has(const char16_t* key) const {
  const Object* const o = as_object(_raw);
  return o != nullptr && o->get(key) != nullptr;
}

bool FrameInfo::same_as(const FrameInfo& other) const { return same_ref(_raw, other._raw); }

const FrameInfo& empty_frame_info() {
  static const FrameInfo info = make_empty_frame();
  return info;
}

const FrameInfo& gone_frame_info() {
  static const FrameInfo info = make_gone_frame();
  return info;
}

}