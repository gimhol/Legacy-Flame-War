#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"

namespace lfw {

class Entity;

namespace collision {

struct CollisionActor {
  std::u16string id;
  double px = 0;
  double py = 0;
  double pz = 0;
  std::u16string data_id;
  double data_type = 0;
  FrameInfo frame;
  Value itr_prefabs;
  Value bear_wpoint_attacking;
  bool marks_group_attack = false;
  bool dropping = false;
  Value arest;
  bool has_catcher = false;
  Value catcher_hurtable;
  Value invulnerable;
  Value bot_ignore;
  Value team;
  Value emitter;
  Value spawn_time;
  bool is_bot_ctrl = false;
};

class ActorHandle {
 public:
  ActorHandle() = default;

  static ActorHandle of(const Entity& e);
  static ActorHandle of(const CollisionActor& a);

  const std::u16string& id() const;
  const Entity* entity() const;

  const CollisionActor& snapshot() const;
  CollisionActor& snap_mut();

  double px() const;
  double py() const;
  double pz() const;
  std::u16string data_id() const;
  double data_type() const;
  const FrameInfo& frame() const;
  Value itr_prefabs() const;
  Value bear_wpoint_attacking() const;
  bool marks_group_attack() const;
  bool dropping() const;
  Value arest() const;
  bool has_catcher() const;
  Value catcher_hurtable() const;
  Value invulnerable() const;
  Value bot_ignore() const;
  Value team() const;
  Value emitter() const;
  Value spawn_time() const;
  bool is_bot_ctrl() const;

 private:
  void ensure_snapshot() const;

  const Entity* _entity = nullptr;
  std::u16string _id;
  mutable CollisionActor _snap;
  mutable bool _snap_built = false;
};

}

}
