#pragma once

#include "lfw/entity/entity.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"

namespace lfw {

namespace helper {

inline double manhattan_xz(const Entity& a, const Entity& b) {
  return round_float(abs(a.position.x - b.position.x) + abs(a.position.z - b.position.z));
}

inline double box_gap_xz(const Entity& a, const Entity& b) {
  const double gap_x = max(0.0, max(a.aabb_min_x - b.aabb_max_x, b.aabb_min_x - a.aabb_max_x));
  const double gap_z = max(0.0, max(a.aabb_min_z - b.aabb_max_z, b.aabb_min_z - a.aabb_max_z));
  return gap_x + gap_z;
}

}

}