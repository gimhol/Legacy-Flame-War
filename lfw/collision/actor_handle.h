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
  const FrameInfo* frame = nullptr;
  Value itr_prefabs;
  std::u16string bear_wpoint_attacking;
  bool marks_group_attack = false;
  bool dropping = false;
  double arest = 0;
  bool has_catcher = false;
  double catcher_hurtable = 0;
  double invulnerable = 0;
  double bot_ignore = 0;
  std::u16string team;
  std::u16string emitter;
  double spawn_time = 0;
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
  const FrameInfo* frame() const;
  Value itr_prefabs() const;
  const std::u16string& bear_wpoint_attacking() const;
  bool marks_group_attack() const;
  bool dropping() const;
  double arest() const;
  bool has_catcher() const;
  double catcher_hurtable() const;
  double invulnerable() const;
  double bot_ignore() const;
  const std::u16string& team() const;
  const std::u16string& emitter() const;
  double spawn_time() const;
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
