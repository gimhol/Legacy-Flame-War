#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/collision/actor_handle.h"
#include "lfw/core/value.h"

namespace lfw {

class World;

namespace collision {

struct Cube {
  double left = 0;
  double right = 0;
  double bottom = 0;
  double top = 0;
  double near = 0;
  double far = 0;
};

struct Collision {
  // 服务根：handler 需要世界级服务时直接 world->...，不再经由一层 std::function
  // 注入的 *Env 结构。
  World* world = nullptr;
  std::u16string id;
  ActorHandle attacker;
  ActorHandle victim;
  Value itr;
  Value bdy;
  Value aframe;
  Value bframe;
  Cube a_cube;
  Cube b_cube;
  double ax = 0;
  double ay = 0;
  double az = 0;
  double vx = 0;
  double vy = 0;
  double vz = 0;
  double dx = 0;
  double dy = 0;
  double dz = 0;
  double m_distance = 0;
  std::u16string adata_id;
  std::u16string vdata_id;
  std::u16string aframe_id;
  std::u16string bframe_id;
  double itr_index = 0;
  double bdy_index = 0;
  Value priority;
  std::shared_ptr<std::vector<std::u16string>> handlers;
  Value injury;
  Value injury_r;
  Value real_injury;
  Value real_injury_r;
  double rest = 0;
  std::u16string aid;
  std::u16string vid;
  Value dataset;
};

struct CollisionInits {
  ActorHandle attacker;
  ActorHandle victim;
  Value itr;
  Value bdy;
  Value aframe;
  Value bframe;
  double itr_index = 0;
  double bdy_index = 0;
};

struct CollisionSnapshot {
  std::u16string aid;
  std::u16string vid;
  std::u16string adata_id;
  std::u16string vdata_id;
  std::u16string aframe_id;
  std::u16string bframe_id;
  double itr_index = 0;
  double bdy_index = 0;
  double ax = 0;
  double ay = 0;
  double az = 0;
  double vx = 0;
  double vy = 0;
  double vz = 0;
  double dx = 0;
  double dy = 0;
  double dz = 0;
  double m_distance = 0;
  double rest = 0;
};

Collision& collision_new(World& world, const CollisionInits& o);
Collision* collision_get(World& world, const ActorHandle& attacker,
                         const ActorHandle& victim);
bool collision_test(Collision& c);
CollisionSnapshot collision_to_snapshot(const Collision& c);
Collision* collision_from_snapshot(World& world, const CollisionSnapshot& s);
void collision_clone(World& world, const Collision& src, Collision& out);

void handle_super_punch_me(Collision& c);
void handle_weapon_picked(Collision& c);
void handle_stiffness(Collision& c);
void handle_body_goto(Collision& c);
void handle_rest(Collision& c);
void handle_itr_kind_magic_flute(Collision& c);

}
}
