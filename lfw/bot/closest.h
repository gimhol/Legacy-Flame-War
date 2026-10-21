#pragma once

#include <vector>

namespace lfw {

class Entity;

namespace bot {

const Entity* closest(const Entity& me, const std::vector<const Entity*>& list);

}
}
