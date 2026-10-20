#pragma once

#include <functional>
#include <string>

#include "lfw/buff/buff.h"
#include "lfw/collision/collision.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

struct HealingEnv {
  std::function<Entity*(const std::u16string& id)> find_entity;
};

const HealingEnv& healing_env();
void set_healing_env(const HealingEnv& env);

void handle_healing(Collision& c);

}

}
