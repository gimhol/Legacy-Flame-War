#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace collision {

Value run_action(const std::u16string& type, const Value& action,
                 Entity& attacker, Entity& victim, const Value& injury,
                 const Value& real_injury);

}

}
