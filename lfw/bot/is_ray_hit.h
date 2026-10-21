#pragma once

#include "lfw/bot/bot_data.h"

namespace lfw {

class Entity;

namespace bot {

bool is_ray_hit(const Entity& a, const Entity& b, const BotRay& ray);

}
}
