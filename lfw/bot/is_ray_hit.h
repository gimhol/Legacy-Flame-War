#pragma once

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace bot {

Value is_ray_hit(const Entity& a, const Entity& b, const Value& ray);

}
}
