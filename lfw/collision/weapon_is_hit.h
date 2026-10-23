#pragma once

#include <string>

#include "lfw/collision/handlers2.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

struct SparkPoint {
  double x = 0;
  double y = 0;
  double z = 0;
};

void handle_weapon_is_hit(Collision& c);

}

}
