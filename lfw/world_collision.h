#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lfw/base/expression.h"
#include "lfw/buff/buff.h"
#include "lfw/collision/action_handlers.h"
#include "lfw/collision/ball_frozen.h"
#include "lfw/collision/collision.h"
#include "lfw/collision/fall.h"
#include "lfw/collision/handlers2.h"
#include "lfw/collision/handlers3.h"
#include "lfw/collision/handlers4.h"
#include "lfw/collision/healing.h"
#include "lfw/collision/keeper.h"
#include "lfw/collision/n_bdy_defend.h"
#include "lfw/collision/n_bdy_normal.h"
#include "lfw/collision/weapon_is_hit.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;
class World;

class WorldCollisionHost {
 public:
  explicit WorldCollisionHost(World& world);
  ~WorldCollisionHost();

  WorldCollisionHost(const WorldCollisionHost&) = delete;
  WorldCollisionHost& operator=(const WorldCollisionHost&) = delete;

  World& world() const { return *_world; }
  const collision::CollisionCoreEnv& core() const { return _core; }
  const collision::CollisionKeeper& keeper() const { return collision::collisions_keeper(); }

  collision::Collision* collision_get(Entity& a, Entity& v);
  bool collision_test(collision::Collision& c);
  void handle(collision::Collision& c);
  void reset_collisions();

  collision::ActorHandle actor_of(Entity& e) const;

 private:
  void ensure_published();
  void publish_globals();
  void clear_globals();

  void bind();
  void bind_core();
  void bind_keeper();
  void bind_handlers();
  void bind_action();
  void bind_handlers2();
  void bind_handlers3();
  void bind_handlers4();
  void bind_fall();
  void bind_nbdy_normal();
  void bind_nbd_defend();
  void bind_weapon_is_hit();
  void bind_ball_frozen();
  void bind_healing();

  static Entity* entity_of_collision_a(const collision::Collision& c);
  static Entity* entity_of_collision_v(const collision::Collision& c);
  Value entity_helpers_value(Entity& e) const;
  Value collision_helpers_value(const collision::Collision& c) const;
  Value world_helpers_value() const;
  bool tester_run(const Value& tester, collision::Collision& c);
  Value tester_debug(const Value& tester);

  void mt_mark(const std::u16string& mark);

  collision::Collision& acquire_collision();
  buff::Buff* find_buff(const std::u16string& id) const;
  buff::Buff* create_buff(const std::u16string& kind, const std::u16string& id);

  World* _world = nullptr;

  collision::CollisionCoreEnv _core;
  collision::KeeperEnv _keeper;
  collision::HandlersEnv _handlers;
  collision::ActionEnv _action;
  collision::Handlers2Env _handlers2;
  collision::Handlers3Env _handlers3;
  collision::Handlers4Env _handlers4;
  collision::FallEnv _fall;
  collision::NbdyNormalEnv _nbdy_normal;
  collision::NbdDefendEnv _nbd_defend;
  collision::WeaponIsHitEnv _weapon_is_hit;
  collision::BallFrozenEnv _ball_frozen;
  collision::HealingEnv _healing;

  std::vector<std::unique_ptr<collision::Collision>> _collisions;
  size_t _collision_at = 0;

  std::map<std::u16string, Expression<collision::Collision>> _testers;
};

}
