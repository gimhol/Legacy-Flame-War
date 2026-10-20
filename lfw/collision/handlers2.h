#pragma once

#include <functional>
#include <string>

#include "lfw/buff/buff.h"
#include "lfw/collision/calc_itr_velocity.h"
#include "lfw/collision/handlers.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

struct Handlers2Env {
  std::function<void(const std::u16string& msg)> warn;
  std::function<Value()> hp_recoverability;
  std::function<Entity*(const std::u16string& id)> find_entity;
  std::function<void(Entity* a, const Value& injury, Entity* v, const Value& prev_hp)>
      summary_apply_damage;
  std::function<bool(const Entity& e)> is_fighter;
  std::function<ItrVelocity(Collision& c)> calc_velocity;
};

void handle_injury(Collision& c, double scale, bool keep_toughness);
void handle_itr_catch(Collision& c);
void handle_itr_kind_freeze(Collision& c);
void handle_itr_effect_freeze(Collision& c);
void handle_john_shield_hit_other_ball(Collision& c);

const Handlers2Env& handlers2_env();
void set_handlers2_env(const Handlers2Env& env);

}

}
