#include "lfw/bot/closest.h"

#include <cstddef>

#include "lfw/entity/entity.h"
#include "lfw/utils/math/base.h"

namespace lfw {
namespace bot {

namespace {

double distance_xz(const Entity& me, const Entity& it) {
  const double dx = me.position.x - it.position.x;
  const double dz = me.position.z - it.position.z;
  return round(abs(dx) + abs(dz));
}

}

const Entity* closest(const Entity& me, const std::vector<const Entity*>& list) {
  const Entity* ret = nullptr;
  double distance = 0;
  for (const Entity* it : list) {
    if (it == nullptr) continue;
    const double d = distance_xz(me, *it);
    if (ret == nullptr || d < distance) {
      ret = it;
      distance = d;
    }
  }
  return ret;
}

}
}
