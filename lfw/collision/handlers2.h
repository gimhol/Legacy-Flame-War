#pragma once

#include <string>

#include "lfw/buff/buff.h"
#include "lfw/collision/calc_itr_velocity.h"
#include "lfw/collision/handlers.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

void handle_injury(Collision& c, double scale, bool keep_toughness);
void handle_itr_catch(Collision& c);
void handle_itr_kind_freeze(Collision& c);
void handle_itr_effect_freeze(Collision& c);
void handle_john_shield_hit_other_ball(Collision& c);

}

}
