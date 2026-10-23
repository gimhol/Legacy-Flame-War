#include "lfw/collision/n_bdy_defend.h"

#include "lfw/collision/action_handlers.h"

#include <variant>

#include "lfw/collision/handlers.h"
#include "lfw/collision/handlers2.h"
#include "lfw/defines/action_type.h"
#include "lfw/defines/collision_defaults.h"
#include "lfw/defines/itr_effect.h"
#include "lfw/defines/spark_enum.h"
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

// 派发到 action（防守动作）：伤害值一律是 null，与 TS 侧一致。
void dispatch(const Collision& c, const std::u16string& handler_type, const Value& action) {
  Entity* const a = lfw::WorldCollisionHost::entity_of_collision_a(c);
  Entity* const v = lfw::WorldCollisionHost::entity_of_collision_v(c);
  if (a == nullptr || v == nullptr) return;
  run_action(handler_type, action, *a, *v, Value(NullTag{}), Value(NullTag{}));
}

bool is_effect(double effect, ItrEffect want) { return effect == static_cast<double>(want); }

void run_actions(const Collision& c, const Value& actions, const char16_t* a_type,
                 const char16_t* v_type, const char16_t* a_dispatch, const char16_t* v_dispatch) {
  const Array* arr = as_array(actions);
  if (arr == nullptr) return;
  for (size_t i = 0; i < arr->size(); ++i) {
    const Value& action = arr->at(i);
    const Value type = field_or(action, u"type");
    if (strict_equals(type, Value(std::u16string(a_type)))) {
      dispatch(c, a_dispatch, action);
    }
    if (strict_equals(type, Value(std::u16string(v_type)))) {
      dispatch(c, v_dispatch, action);
    }
  }
}

}

void handle_itr_normal_bdy_defend(Collision& c) {
  Entity* a = world_of(c).find_entity(c.aid);
  Entity* v = world_of(c).find_entity(c.vid);
  if (a == nullptr || v == nullptr) return;

  const double effect_v = c.itr->has_effect() ? c.itr->effect() : 0.0;
  const double bdefend = c.itr->has_bdefend() ? c.itr->bdefend() : kDefaultBreakDefendValue;

  const bool explosive =
      is_effect(effect_v, ItrEffect::FireExplosion) || is_effect(effect_v, ItrEffect::Explosion);
  if ((!explosive && strict_equals(Value(a->facing), Value(v->facing))) ||
      bdefend >= kDefaultForceBreakDefendValue) {
    handle_itr_normal_bdy_normal(c);
    return;
  }

  v->set_defend_value(v->defend_value() - bdefend);
  handle_injury(c, v->defend_ratio(), false);
  handle_rest(c);
  handle_stiffness(c);

  double x = 0;
  double y = 0;
  double z = 0;
  v->spark_point(c.a_cube, c.b_cube, x, y, z);
  const ItrVelocity iv = host_of(c).calc_velocity(c);
  if (truthy(Value(iv.x))) v->set_velocity(Value(iv.x / 2.0), Value(), Value());

  const Value& itr_actions = c.itr->actions();
  const Value& bdy_actions = c.bdy->actions();

  if (v->defend_value() <= 0) {
    v->set_defend_value(0.0);
    host_of(c).spark(x, y, z, spark_enum::kBrokenDefend);
    run_actions(c, itr_actions, action_type::kA_DEFEND, action_type::kV_DEFEND,
                action_type::kA_NEXT_FRAME, action_type::kV_NEXT_FRAME);
    run_actions(c, bdy_actions, action_type::kA_BROKEN_DEFEND, action_type::kV_BROKEN_DEFEND,
                action_type::kA_NEXT_FRAME, action_type::kV_NEXT_FRAME);
  } else {
    host_of(c).spark(x, y, z, spark_enum::kDefendHit);
    run_actions(c, itr_actions, action_type::kA_DEFEND, action_type::kV_DEFEND,
                action_type::kA_NEXT_FRAME, action_type::kV_NEXT_FRAME);
    run_actions(c, bdy_actions, action_type::kA_DEFEND, action_type::kV_DEFEND,
                action_type::kA_NEXT_FRAME, action_type::kV_NEXT_FRAME);
  }
}

}
}
