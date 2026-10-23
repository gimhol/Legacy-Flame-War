#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/collision/actor_handle.h"
#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"

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
  const ItrInfo* itr = nullptr;
  const BdyInfo* bdy = nullptr;
  const FrameInfo* aframe = nullptr;
  const FrameInfo* bframe = nullptr;
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
  std::optional<double> priority;
  std::shared_ptr<std::vector<std::u16string>> handlers;
  std::optional<double> injury;
  std::optional<double> injury_r;
  std::optional<double> real_injury;
  std::optional<double> real_injury_r;
  double rest = 0;
  std::u16string aid;
  std::u16string vid;
};

struct CollisionInits {
  ActorHandle attacker;
  ActorHandle victim;
  const ItrInfo* itr = nullptr;
  const BdyInfo* bdy = nullptr;
  const FrameInfo* aframe = nullptr;
  const FrameInfo* bframe = nullptr;
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
bool collision_precheck(World& world, const ActorHandle& a, const ActorHandle& v,
                        const ItrInfo& itr, const BdyInfo& bdy, const Cube& a_cube,
                        const Cube& b_cube, bool rest, double itr_index, double bdy_index,
                        const std::u16string& aid);
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
