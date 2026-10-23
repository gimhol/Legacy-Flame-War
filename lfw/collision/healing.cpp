#include "lfw/collision/healing.h"

#include "lfw/buff/buff_healing.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/world.h"

namespace lfw {
namespace collision {
void handle_healing(Collision& c) {
  if (!c.itr->has_injury() || !truthy(c.itr->injury())) return;
  const double injury = c.itr->injury();

  World& world = *c.world;
  Entity* a = world.find_entity(c.aid);
  Entity* v = world.find_entity(c.vid);
  if (v == nullptr) return;

  buff::grant_buff(u"Healing", a, v, buff::Buff_Healing::duration_of(*v, injury));
}

}
}
