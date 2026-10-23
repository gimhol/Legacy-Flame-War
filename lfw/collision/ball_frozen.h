#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/data/hit_area_info.h"

namespace lfw {

class Entity;

namespace collision {

bool handle_ball_frozen(Entity& a, Entity& v, const ItrInfo& itr);

}

}
