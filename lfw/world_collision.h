#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lfw/base/expression.h"
#include "lfw/buff/buff.h"
#include "lfw/collision/calc_itr_velocity.h"
#include "lfw/collision/collision.h"
#include "lfw/collision/keeper.h"
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
  const collision::CollisionKeeper& keeper() const { return collision::collisions_keeper(); }

  collision::Collision* collision_get(Entity& a, Entity& v);
  bool collision_test(collision::Collision& c);
  void handle(collision::Collision& c);
  void reset_collisions();

  collision::ActorHandle actor_of(Entity& e) const;

  // —— 碰撞 handler 直接调用的服务。原先这些是一个个 std::function 字段挂在 *Env 结构
  // 上、由本类注入的，现在就是普通成员函数，谁需要谁直接调 —— 
  collision::Cube get_bounding(const collision::ActorHandle& a, const Value& frame,
                               const Value& box) const;
  bool victim_get_v_rest(const collision::Collision& c) const;
  bool attacker_is_ally(const collision::Collision& c) const;
  collision::Collision& acquire_collision();
  std::u16string new_id() const;
  bool dev() const;
  void log(const std::u16string& msg) const;
  Value tester_debug(const Value& tester);
  bool tester_run(const Value& tester, collision::Collision& c);
  bool find_actor(const std::u16string& id, collision::ActorHandle& out) const;
  bool find_object_data(const std::u16string& id, Value& out) const;
  static Value priority_of(double data_type);
  bool load_handlers(collision::Collision& c) const;

  void spark(const Value& x, const Value& y, const Value& z, const Value& type) const;
  void play_sound_global(const Value& sounds, double x, double y, double z) const;
  Value hp_recoverability() const;
  void summary_apply_damage(Entity* a, const Value& injury, Entity* v, const Value& prev_hp);
  double mt_int() const;
  Value mt_pick(const Value& indexes) const;
  void mt_mark(const std::u16string& mark);
  bool find_data(const std::u16string& oid, Value& out) const;
  void broadcast(const std::u16string& msg) const;
  void alert(const std::u16string& msg) const;
  collision::ItrVelocity calc_velocity(collision::Collision& c) const;
  bool is_fall(collision::Collision& c) const;
  bool is_armor_work(collision::Collision& c) const;
  void run_action(const std::u16string& type, const Value& action, collision::Collision& c);

  static Entity* entity_of_collision_a(const collision::Collision& c);
  static Entity* entity_of_collision_v(const collision::Collision& c);

  buff::Buff* find_buff(const std::u16string& id) const;
  buff::Buff* create_buff(const Value& kind, const std::u16string& id);

 private:
  World* _world = nullptr;

  std::vector<std::unique_ptr<collision::Collision>> _collisions;
  size_t _collision_at = 0;

  std::map<std::u16string, Expression<collision::Collision>> _testers;
};

}
