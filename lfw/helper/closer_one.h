#pragma once

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace helper {

Value closer_one(const Value& s, const Value& t1, const Value& t2);
Value closer_one(const Entity& s, const Value& t1, const Value& t2);

}
}
