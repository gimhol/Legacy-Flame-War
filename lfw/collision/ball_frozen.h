#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

bool handle_ball_frozen(Entity& a, Entity& v, const Value& itr);

}

}
