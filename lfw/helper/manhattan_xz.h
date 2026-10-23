#pragma once

#include "lfw/entity/entity.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"

namespace lfw {

namespace helper {

inline double manhattan_xz(const Entity& a, const Entity& b) {
  return round_float(abs(a.position.x - b.position.x) + abs(a.position.z - b.position.z));
}

}

}