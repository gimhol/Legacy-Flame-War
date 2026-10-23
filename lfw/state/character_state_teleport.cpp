#include "lfw/state/character_state_teleport.h"

#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/utils/math/base.h"
#include "lfw/world.h"

namespace lfw {
namespace state {
namespace {

Entity* pick_target(Entity& e, bool nearest_enemy) {
  double best = -1;
  Entity* tar = nullptr;
  for (Entity* const o : e.world().entities) {
    if (o == nullptr) continue;
    if (!entity::is_fighter(*o) || o == &e) continue;
    if (nearest_enemy ? o->is_ally(e) : !o->is_ally(e)) continue;
    if (o->hp() <= 0) continue;
    const double mx = e.position.x;
    const double dis = abs(o->position.x - mx);
    const bool better = nearest_enemy ? (best < 0 || dis < best) : (dis > best);
    if (better) {
      best = dis;
      tar = o;
    }
  }
  return tar;
}

void teleport_to(Entity& e, const Entity* tar) {
  double x = e.position.x;
  double z = e.position.z;
  if (tar != nullptr) {
    x = round(tar->position.x - e.facing * 120);
    z = round(tar->position.z);
  }
  const Value segment = e.ground_segment(x, z);
  const double gy = e.ground_y(segment, x, z);
  e.set_position(Value(x), Value(gy), Value(z));
}

void cst_nearest_enter(Entity& e, const FrameInfo* prev_frame) {
  (void)prev_frame;
  teleport_to(e, pick_target(e, true));
}

void cst_farthest_enter(Entity& e, const FrameInfo* prev_frame) {
  (void)prev_frame;
  teleport_to(e, pick_target(e, false));
}

}

CharacterState_Teleport2NearestEnemy::CharacterState_Teleport2NearestEnemy(Value state)
    : CharacterState_Base(std::move(state)) {
  enter = &cst_nearest_enter;
}

CharacterState_Teleport2FarthestAlly::CharacterState_Teleport2FarthestAlly(Value state)
    : CharacterState_Base(std::move(state)) {
  enter = &cst_farthest_enter;
}

}
}
