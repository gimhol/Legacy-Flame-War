#include "lfw/collision/handlers4.h"

#include "lfw/collision/handlers.h"
#include "lfw/defines/bdy_kind.h"
#include "lfw/defines/collision_defaults.h"
#include "lfw/defines/frame_behavior.h"
#include "lfw/defines/state_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"

namespace lfw {
namespace collision {
namespace {

lfw::WorldCollisionHost& host_of(const Collision& c) { return c.world->collision_host(); }

lfw::World& world_of(const Collision& c) { return host_of(c).world(); }

void zero_hp(Entity& e) {
  e.set_hp_r(0.0);
  e.set_hp(0.0);
}

bool state_is(const Value& state, StateEnum want) {
  return strict_equals(state, Value(static_cast<double>(want)));
}

}

void handle_ball_hit_other(Collision& c) {
  handle_rest(c);
  handle_stiffness(c);
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;
  if (strict_equals(c.aframe->behavior(),
                    Value(static_cast<double>(FrameBehavior::JohnChase)))) {
    if (entity::is_fighter_data(v->data())) {
      const double kind = c.bdy->has_kind() ? c.bdy->kind() : 0.0;
      if (kind == static_cast<double>(BdyKind::Normal)) {
        zero_hp(*a);
      } else if (kind == static_cast<double>(BdyKind::Defend)) {
        const double bdefend = c.itr->has_bdefend() ? c.itr->bdefend() : 0.0;
        if (bdefend >= kDefaultForceBreakDefendValue) {
          zero_hp(*a);
        } else if (strict_equals(Value(v->facing), Value(a->facing))) {
          zero_hp(*a);
        }
      }
    }
  }
  a->play_sound(a->data_base_hit_sounds());
}

void handle_weapon_hit_other(Collision& c) {
  Entity* a = world_of(c).find_entity(c.aid);
  if (a == nullptr) return;
  if (state_is(a->state(), StateEnum::Weapon_OnHand)) return;
  const Value base_type = Value(a->base_type());
  const bool is_base_ball =
      strict_equals(base_type, Value(static_cast<double>(WeaponEnum::Baseball))) ||
      strict_equals(base_type, Value(static_cast<double>(WeaponEnum::Drink)));
  if (!state_is(a->state(), StateEnum::Weapon_Throwing)) return;
  if (is_base_ball) {
    a->set_velocity(Value(0.0), Value(5.0), Value(0.0));
  } else {
    const double vx = a->velocity.x;
    const double vy = a->velocity.y;
    const double vz = a->velocity.z;
    (void)vz;
    a->set_velocity(Value(-0.3 * vx), Value(0.3 * vy), Value(0.0));
  }
  const Value nf = a->find_align_frame(to_string(a->frame_id()), a->data_indexes_of(u"throwings"),
                                      a->data_indexes_in_the_skys());
  const Value arest = Value(a->arest());
  a->enter_frame(nf);
  a->set_arest(to_number(arest));
  a->set_dropping(true);
}

}
}
