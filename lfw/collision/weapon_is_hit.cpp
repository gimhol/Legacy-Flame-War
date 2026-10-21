#include "lfw/collision/weapon_is_hit.h"

#include "lfw/collision/handlers.h"
#include "lfw/defines/collision_defaults.h"
#include "lfw/defines/oid.h"
#include "lfw/defines/spark_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"

namespace lfw {
namespace collision {
namespace {

lfw::WorldCollisionHost& host_of(const Collision& c) { return c.world->collision_host(); }

lfw::World& world_of(const Collision& c) { return host_of(c).world(); }

Value index_0(const Value& holder) {
  const Array* arr = as_array(holder);
  if (arr != nullptr) {
    if (arr->size() == 0) return Value();
    return arr->at(0);
  }
  const Object* o = as_object(holder);
  if (o != nullptr) {
    const Value* p = o->get(u"0");
    if (p != nullptr) return *p;
  }
  return Value();
}

// 火花点取受击方身上算出来的位置；受击方不在（快照/已消失）时归零。
SparkPoint spark_point_of(const Collision& c, const Cube& a, const Cube& b) {
  SparkPoint sp;
  double x = 0;
  double y = 0;
  double z = 0;
  const Entity* const v = c.victim.entity();
  if (v != nullptr) const_cast<Entity*>(v)->spark_point(a, b, x, y, z);
  sp.x = Value(x);
  sp.y = Value(y);
  sp.z = Value(z);
  return sp;
}

void zero_hp(Entity& e) {
  e.set_hp_r(0.0);
  e.set_hp(0.0);
}

}

void handle_weapon_is_hit(Collision& c) {
  handle_rest(c);
  handle_stiffness(c);
  handle_injury(c, 1.0, false);

  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;

  v->set_dropping(false);
  const Value bdefend = field_or(c.itr, u"bdefend");
  if (truthy(bdefend) && ge(bdefend, Value(kDefaultForceBreakDefendValue))) {
    zero_hp(*v);
  }

  const Value fall = field_or(c.itr, u"fall");
  const bool is_fly = truthy(fall) && ge(fall, Value(kDefaultFallValueCritical));

  const SparkPoint sp = spark_point_of(c, c.a_cube, c.b_cube);
  host_of(c).spark(sp.x, sp.y, sp.z,
              is_fly ? spark_enum::kSilentCriticalHit : spark_enum::kSilentHit);

  const ItrVelocity iv = host_of(c).calc_velocity(c);
  double vx = iv.x;
  const double vy = iv.y;
  const double vz = iv.z;
  const Value base_type = Value(v->base_type());
  const bool is_base_ball =
      strict_equals(base_type, Value(static_cast<double>(WeaponEnum::Baseball))) ||
      strict_equals(base_type, Value(static_cast<double>(WeaponEnum::Drink)));

  if (!strict_equals(base_type, Value(static_cast<double>(WeaponEnum::Heavy))) || is_fly) {
    v->set_velocity(Value(vx), Value(vy), Value(vz));
    v->leave_ground();
    host_of(c).mt_mark(u"hwih_1");
    Value nid;
    if (is_base_ball && (vx >= 6 || vx <= -6)) {
      nid = host_of(c).mt_pick(v->data_indexes_of(u"throwings"));
    } else {
      nid = host_of(c).mt_pick(v->data_indexes_in_the_skys());
    }
    v->enter_frame_by_id(nid);
  }

  if (strict_equals(a->data_id(), Value(oid::kWeapon_Stick)) && is_base_ball) {
    vx = a->facing * 2;
    host_of(c).mt_mark(u"hwih_2");
    v->enter_frame_by_id(index_0(v->data_indexes_of(u"throwings")));
    v->set_velocity(Value(vx), Value(), Value());
  }

  if (is_fly && v->bearer == nullptr) {
    v->set_team(a->team());
  }
}

}
}
