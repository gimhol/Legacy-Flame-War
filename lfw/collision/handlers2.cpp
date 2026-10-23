#include "lfw/collision/handlers2.h"

#include <variant>

#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"
#include "lfw/utils/math/base.h"

namespace lfw {
namespace collision {
namespace {

lfw::WorldCollisionHost& host_of(const Collision& c) { return c.world->collision_host(); }

lfw::World& world_of(const Collision& c) { return host_of(c).world(); }

bool missing(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

}

void handle_injury(Collision& c, double scale, bool keep_toughness) {
  const Value injury_v = field_or(c.itr->to_value(), u"injury");
  if (!truthy(injury_v)) return;
  const double injury = round(to_number(injury_v) * scale);
  if (!injury) return;
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (v == nullptr) return;
  const double prev_hp = v->hp();
  const double prev_hp_r = v->hp_r();
  v->set_hp(prev_hp - injury);
  const double injury_r = round(injury * (1 - to_number(host_of(c).hp_recoverability())));
  if (injury_r) v->set_hp_r(prev_hp_r - injury_r);
  const double real_injury = prev_hp - v->hp();
  const double real_injury_r = prev_hp_r - v->hp_r();
  if (!keep_toughness) v->set_toughness(0.0);
  Entity* att = a;
  const std::u16string* const se = a == nullptr ? nullptr : a->src_emitter();
  if (se != nullptr && !se->empty()) att = world_of(c).find_entity(*se);
  c.injury = injury;
  c.injury_r = injury_r;
  c.real_injury = real_injury;
  c.real_injury_r = real_injury_r;
  if (att == nullptr) return;
  host_of(c).summary_apply_damage(att, Value(injury), v, Value(prev_hp));
  if (att->marks_has(u"Electrify") && entity::is_fighter_data(v->data())) {
    const Value elec = att->dataset(u"electrify_duration");
    buff::grant_buff(u"Electroshock", att, v, missing(elec) ? 0 : to_number(elec));
  }
}

void handle_itr_catch(Collision& c) {
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;
  if (a->catching != nullptr) return;
  if (v->catcher != nullptr) return;
  a->set_catch_time(a->catch_time_max());
  a->set_catching(v);
  const Value catchingact = field_or(c.itr->to_value(), u"catchingact");
  if (truthy(catchingact))
    a->enter_frame(catchingact);
  else
    host_of(c).alert(u"[handle_itr_catch] catchingact got " + to_string(catchingact));
  v->set_catcher(a);
  v->set_resting(0.0);
  v->set_fall_value(v->fall_value_max());
  v->set_defend_value(v->defend_value_max());
  const Value caughtact = field_or(c.itr->to_value(), u"caughtact");
  if (truthy(caughtact))
    v->enter_frame(caughtact);
  else
    host_of(c).alert(u"[handle_itr_catch] caughtact got " + to_string(caughtact));
}

void handle_itr_kind_freeze(Collision& c) {
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;
  v->set_fall_value(v->fall_value() - to_number(a->itr_fall(c.itr->to_value())));
  handle_injury(c, 1, false);
  handle_rest(c);
  handle_stiffness(c);
  v->enter_frame_by_id(v->data_indexes_of(u"ice"));
}

void handle_itr_effect_freeze(Collision& c) {
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;
  v->set_fall_value(v->fall_value() - to_number(a->itr_fall(c.itr->to_value())));
  const ItrVelocity vel = host_of(c).calc_velocity(c);
  v->set_velocity(Value(vel.x), Value(vel.y), Value(vel.z));
  handle_injury(c, 1, false);
  handle_rest(c);
  handle_stiffness(c);
  v->enter_frame_by_id(v->data_indexes_of(u"ice"));
}

void handle_john_shield_hit_other_ball(Collision& c) {
  handle_rest(c);
  handle_injury(c, 1, false);
  handle_stiffness(c);
  Entity* a = world_of(c).find_entity(c.aid);
  if (a == nullptr) return;
  a->set_shaking(a->motionless);
  a->play_sound(a->data_base_hit_sounds());
}

}
}
