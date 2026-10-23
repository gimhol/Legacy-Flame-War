#include "lfw/collision/fall.h"

#include "lfw/defines/collision_defaults.h"
#include "lfw/defines/itr_effect.h"
#include "lfw/defines/spark_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/face_helper.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"

namespace lfw {
namespace collision {
namespace {

lfw::WorldCollisionHost& host_of(const Collision& c) { return c.world->collision_host(); }

lfw::World& world_of(const Collision& c) { return host_of(c).world(); }

Value index_by(const Value& holder, const std::u16string& k) {
  const Object* o = as_object(holder);
  if (o != nullptr) {
    const Value* p = o->get(k);
    if (p != nullptr) return *p;
  }
  const Array* arr = as_array(holder);
  if (arr != nullptr) {
    const double index = to_number(Value(k));
    if (!(index >= 0) || index >= static_cast<double>(arr->size())) return Value();
    return arr->at(static_cast<size_t>(index));
  }
  return Value();
}

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

bool effect_is(double effect, ItrEffect want) { return effect == static_cast<double>(want); }

}

void handle_fall(Collision& c) {
  Entity* v = world_of(c).find_entity(c.vid);
  if (v == nullptr) return;

  v->set_toughness(0.0);
  v->set_fall_value(0.0);
  v->set_defend_value(0.0);
  v->set_resting(0.0);

  const ItrVelocity iv = host_of(c).calc_velocity(c);
  v->set_velocity(Value(iv.x), Value(iv.y), Value(iv.z));

  const double fall = c.itr->has_fall() ? c.itr->fall() : 0.0;
  const bool is_critical = truthy(fall) && fall >= kDefaultFallValueCritical;

  double sx = 0;
  double sy = 0;
  double sz = 0;
  v->spark_point(c.a_cube, c.b_cube, sx, sy, sz);

  const bool v_is_fighter = entity::is_fighter_data(v->data());
  const double effect_v = c.itr->has_effect() ? c.itr->effect() : 0.0;
  const bool is_sharp = effect_is(effect_v, ItrEffect::Sharp);

  const char16_t* effect = spark_enum::kHit;
  if (v_is_fighter && is_sharp && is_critical) {
    effect = spark_enum::kCriticalBleed;
  } else if (v_is_fighter && is_sharp) {
    effect = spark_enum::kBleedFall;
  } else if (is_critical) {
    effect = spark_enum::kCriticalHit;
  } else {
    effect = spark_enum::kHitFall;
  }

  host_of(c).spark(sx, sy, sz, std::u16string(effect));

  const Value fire = v->data_indexes_of(u"fire");
  const Value critical_hit = v->data_indexes_of(u"critical_hit");

  auto normal_fall_act = [&]() {
    if (!truthy(critical_hit)) return;
    const double direction = v->velocity.x / v->facing >= 0 ? 1.0 : -1.0;
    v->enter_frame_by_id(index_0(index_by(critical_hit, to_string(Value(direction)))));
  };

  if (effect_is(effect_v, ItrEffect::Fire) || effect_is(effect_v, ItrEffect::MFire2)) {
    if (truthy(fire)) {
      if (strict_equals(v->holding_base_type(), Value(static_cast<double>(WeaponEnum::Heavy)))) {
        v->drop_holding();
      }
      auto info = std::make_shared<Object>();
      info->set(u"id", index_0(fire));
      info->set(u"facing", entity::turn_face(iv.x_direction));
      v->enter_frame(Value(info));
    } else {
      normal_fall_act();
    }
  } else if (effect_is(effect_v, ItrEffect::MFire1) ||
             effect_is(effect_v, ItrEffect::FireExplosion)) {
    if (truthy(fire)) {
      if (strict_equals(v->holding_base_type(), Value(static_cast<double>(WeaponEnum::Heavy)))) {
        v->drop_holding();
      }
      auto info = std::make_shared<Object>();
      info->set(u"id", index_0(fire));
      info->set(u"facing", iv.x_direction);
      v->enter_frame(Value(info));
    } else {
      normal_fall_act();
    }
  } else {
    normal_fall_act();
  }
}

}
}
