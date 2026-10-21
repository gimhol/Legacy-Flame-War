#pragma once

#include <string>

#include "lfw/collision/collision.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

void handle_ball_hit_other(Collision& c);
void handle_weapon_hit_other(Collision& c);

}

}
