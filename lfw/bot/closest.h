#pragma once

#include <vector>

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace bot {

Value closest(const Value& me, const std::vector<Value>& list);
Value closest(const Entity& me, const std::vector<Value>& list);

}
}
