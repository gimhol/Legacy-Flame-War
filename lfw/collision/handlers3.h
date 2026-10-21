#pragma once

#include <string>

#include "lfw/buff/buff.h"
#include "lfw/collision/collision.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

void handle_itr_kind_whirlwind(Collision& c);
void handle_ball_is_hit_a(Collision& c);
void handle_ball_is_hit_b(Collision& c);
bool handle_armor(Collision& c);

}

}
