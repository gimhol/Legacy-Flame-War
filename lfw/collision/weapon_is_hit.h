#pragma once

#include <string>

#include "lfw/collision/handlers2.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

struct SparkPoint {
  Value x;
  Value y;
  Value z;
};

void handle_weapon_is_hit(Collision& c);

}

}
