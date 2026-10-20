#pragma once

#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace helper {

double manhattan_xz(const Value& a, const Value& b);
double manhattan_xz(const Entity& a, const Value& b);
double manhattan_xz(const Entity& a, const Entity& b);

}
}
