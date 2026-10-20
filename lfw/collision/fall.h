#pragma once

#include <functional>
#include <string>

#include "lfw/collision/handlers2.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

struct FallEnv {
  std::function<Entity*(const std::u16string& id)> find_entity;
  std::function<bool(const Entity& e)> is_fighter;
  std::function<void(const Value& x, const Value& y, const Value& z, const Value& type)> spark;
  std::function<ItrVelocity(Collision& c)> calc_velocity;
};

void handle_fall(Collision& c);

const FallEnv& fall_env();
void set_fall_env(const FallEnv& env);

}

}
