#include "lfw/collision/collision.h"

#include <cstdint>
#include <variant>

#include "lfw/defines/hit_flag.h"
#include "lfw/defines/itr_kind.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/container_help/spread_assign.h"
#include "lfw/utils/math/base.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"

namespace lfw {
namespace collision {
namespace {

bool missing(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool itr_kind_is(const Value& itr, ItrKind k) {
  return strict_equals(field_or(itr, u"kind"), Value(static_cast<double>(k)));
}

bool bit_and(double x, double y) {
  return (static_cast<int32_t>(x) & static_cast<int32_t>(y)) != 0;
}

const Array* array_of(const Value& v) { return as_array(v); }

size_t array_len(const Value& v) {
  const Array* a = array_of(v);
  return a == nullptr ? 0 : a->size();
}

bool array_at(const Value& v, size_t index, Value& out) {
  const Array* a = array_of(v);
  if (a == nullptr || index >= a->size()) return false;
  out = a->at(index);
  return true;
}

bool index_by(const Value& holder, const std::u16string& k, Value& out) {
  const Object* o = as_object(holder);
  if (o != nullptr) {
    const Value* p = o->get(k);
    if (p == nullptr) return false;
    out = *p;
    return true;
  }
  const Array* a = as_array(holder);
  if (a == nullptr) return false;
  const double index = to_number(Value(k));
  if (!(index >= 0) || index >= static_cast<double>(a->size())) return false;
  out = a->at(static_cast<size_t>(index));
  return true;
}

}

namespace {

struct ItrPrep {
  Cube a_cube;
  const ItrInfo* itr = nullptr;
  double itr_index = 0;
  double rest = 0;
};

bool cube_hit(const Cube& ac, const Cube& bc) {
  return !(ac.left > bc.right || ac.right < bc.left || ac.bottom > bc.top || ac.top < bc.bottom ||
           ac.far > bc.near || ac.near < bc.far);
}

ItrPrep prepare_itr(World& world, const ActorHandle& a, const FrameInfo* aframe,
                    const ItrInfo& itr_info, double itr_index) {
  WorldCollisionHost& host = world.collision_host();
  ItrPrep p;
  p.itr = &itr_info;
  p.a_cube = host.get_bounding(a, aframe, itr_info);

  double idx = itr_index;
  if (itr_info.has_kind() && itr_info.kind() == static_cast<double>(ItrKind::WeaponSwing)) {
    const std::u16string& prefab_id = a.bear_wpoint_attacking();
    Value prefab;
    if (prefab_id.empty())
      idx = -1;
    else if (!index_by(a.itr_prefabs(), prefab_id, prefab))
      idx = -1;
    else
      p.itr = host.expanded_itr(itr_info, prefab);
  }
  p.itr_index = idx;

  const Value& itr = p.itr->to_value();
  const WorldDataset& dataset = world.dataset;
  const double min_vrest = dataset.min_vrest();
  const double vrest_offset = dataset.vrest_offset();
  double rest = 0;
  if (!truthy(field_or(itr, u"arest")) && truthy(field_or(itr, u"vrest"))) {
    rest = max(min_vrest, to_number(field_or(itr, u"vrest")) + vrest_offset);
  } else if (itr_kind_is(itr, ItrKind::Normal) && a.marks_group_attack()) {
    const Value ar = field_or(itr, u"arest");
    const double base = truthy(ar) ? to_number(ar) : dataset.itr_arest();
    rest = max(min_vrest, base + vrest_offset);
  }
  p.rest = rest;
  return p;
}

Collision& collision_commit(World& world, const CollisionInits& o, const ItrPrep& p,
                            const Cube& b_cube, const std::u16string& id) {
  WorldCollisionHost& host = world.collision_host();
  const ActorHandle& a = o.attacker;
  const ActorHandle& v = o.victim;
  const double ax = a.px();
  const double ay = a.py();
  const double az = a.pz();
  const double vx = v.px();
  const double vy = v.py();
  const double vz = v.pz();
  const double dx = vx - ax;
  const double dy = vy - ay;
  const double dz = vz - az;

  Collision& c = host.acquire_collision();
  c.id = id;
  c.world = &world;
  c.attacker = a;
  c.victim = v;
  c.itr = p.itr;
  c.bdy = o.bdy;
  c.aframe = o.aframe;
  c.bframe = o.bframe;
  c.ax = ax;
  c.ay = ay;
  c.az = az;
  c.vx = vx;
  c.vy = vy;
  c.vz = vz;
  c.dx = dx;
  c.dy = dy;
  c.dz = dz;
  c.adata_id = a.data_id();
  c.vdata_id = v.data_id();
  c.aframe_id = to_string(o.aframe->id());
  c.bframe_id = to_string(o.bframe->id());
  c.itr_index = p.itr_index;
  c.bdy_index = o.bdy_index;
  c.m_distance = abs(dx) + abs(dy) + abs(dz);
  c.a_cube = p.a_cube;
  c.b_cube = b_cube;
  c.rest = p.rest;
  if (c.handlers)
    c.handlers->clear();
  else
    c.handlers = std::make_shared<std::vector<std::u16string>>();
  c.priority = WorldCollisionHost::priority_of(a.data_type());
  c.injury.reset();
  c.injury_r.reset();
  c.real_injury.reset();
  c.real_injury_r.reset();
  c.aid = a.id();
  c.vid = v.id();
  return c;
}

}  // namespace

Collision& collision_new(World& world, const CollisionInits& o) {
  WorldCollisionHost& host = world.collision_host();
  const ItrPrep p = prepare_itr(world, o.attacker, o.aframe, *o.itr, o.itr_index);
  const Cube b_cube = host.get_bounding(o.victim, o.bframe, *o.bdy);
  const std::u16string id = p.rest ? host.new_id() : o.attacker.id();
  return collision_commit(world, o, p, b_cube, id);
}

bool collision_test(Collision& c) {
  WorldCollisionHost& host = c.world->collision_host();
  if (c.bdy_index < 0) return false;
  if (c.itr_index < 0) return false;
  const ActorHandle& a = c.attacker;
  const ActorHandle& v = c.victim;
  if (a.dropping()) return false;
  if (!c.rest && truthy_number(a.arest())) return false;
  if (c.rest && host.victim_get_v_rest(c)) return false;

  const double itr_kind = c.itr->kind();
  if (itr_kind != static_cast<double>(ItrKind::Heal)) {
    if (truthy_number(v.invulnerable())) return false;
    if (v.has_catcher() && !strict_equals(v.catcher_hurtable(), Value(1.0))) return false;
  }

  if ((itr_kind == static_cast<double>(ItrKind::Pick) ||
       itr_kind == static_cast<double>(ItrKind::PickSecretly)) &&
      strict_equals(v.bot_ignore(), Value(1.0)) && a.is_bot_ctrl())
    return false;

  const Cube& ac = c.a_cube;
  const Cube& bc = c.b_cube;
  if (ac.left > bc.right || ac.right < bc.left || ac.bottom > bc.top || ac.top < bc.bottom ||
      ac.far > bc.near || ac.near < bc.far)
    return false;

  const double ally_flag = host.attacker_is_ally(c) ? static_cast<double>(HitFlag::Ally)
                                                      : static_cast<double>(HitFlag::Enemy);
  const double bdy_flag = c.bdy->has_hit_flag() ? c.bdy->hit_flag()
                                               : static_cast<double>(HitFlag::AllEnemy);
  const double itr_flag = c.itr->has_hit_flag() ? c.itr->hit_flag()
                                                : static_cast<double>(HitFlag::AllEnemy);
  if (!bit_and(itr_flag, v.data_type()) || !bit_and(bdy_flag, a.data_type()) ||
      (!bit_and(itr_flag, ally_flag) && !bit_and(bdy_flag, ally_flag)))
    return false;

  if (v.team() == a.team() && !v.emitter().empty() && v.emitter() == a.emitter() &&
      v.spawn_time() == a.spawn_time())
    return false;

  const Value& bdy_tester = c.bdy->tester();
  if (truthy(bdy_tester)) {
    const bool ret = host.tester_run(bdy_tester, c);
    if (host.dev()) host.log(u"bdy.__tester: " + to_string(host.tester_debug(bdy_tester)));
    if (!ret) return false;
  }
  const Value& itr_tester = c.itr->tester();
  if (truthy(itr_tester)) {
    const bool ret = host.tester_run(itr_tester, c);
    if (host.dev()) host.log(u"itr.__tester: " + to_string(host.tester_debug(itr_tester)));
    if (!ret) return false;
  }
  return host.load_handlers(c);
}

Collision* collision_get(World& world, const ActorHandle& attacker, const ActorHandle& victim) {
  WorldCollisionHost& host = world.collision_host();
  const FrameInfo* const aframe = attacker.frame();
  const FrameInfo* const bframe = victim.frame();
  const std::vector<ItrInfo>& itrs = aframe->itr();
  const std::vector<BdyInfo>& bdys = bframe->bdy();
  const size_t ni = itrs.size();
  const size_t nj = bdys.size();
  if (!ni || !nj) return nullptr;

  std::vector<Cube> b_cubes(nj);
  for (size_t j = 0; j < nj; ++j) {
    b_cubes[j] = host.get_bounding(victim, bframe, bdys[j]);
  }

  for (size_t i = 0; i < ni; ++i) {
    const ItrPrep prep = prepare_itr(world, attacker, aframe, itrs[i], static_cast<double>(i));
    for (size_t j = 0; j < nj; ++j) {
      std::u16string fresh_id;
      const std::u16string* cid = &attacker.id();
      if (prep.rest) {
        fresh_id = host.new_id();
        cid = &fresh_id;
      }
      if (!cube_hit(prep.a_cube, b_cubes[j])) continue;
      CollisionInits inits;
      inits.attacker = attacker;
      inits.victim = victim;
      inits.itr = prep.itr;
      inits.bdy = &bdys[j];
      inits.aframe = aframe;
      inits.bframe = bframe;
      inits.itr_index = prep.itr_index;
      inits.bdy_index = static_cast<double>(j);
      Collision& c = collision_commit(world, inits, prep, b_cubes[j], *cid);
      if (!collision_test(c)) continue;
      return &c;
    }
  }
  return nullptr;
}

CollisionSnapshot collision_to_snapshot(const Collision& c) {
  CollisionSnapshot s;
  s.aid = c.aid;
  s.vid = c.vid;
  s.adata_id = c.adata_id;
  s.vdata_id = c.vdata_id;
  s.aframe_id = c.aframe_id;
  s.bframe_id = c.bframe_id;
  s.itr_index = c.itr_index;
  s.bdy_index = c.bdy_index;
  s.ax = c.ax;
  s.ay = c.ay;
  s.az = c.az;
  s.vx = c.vx;
  s.vy = c.vy;
  s.vz = c.vz;
  s.dx = c.dx;
  s.dy = c.dy;
  s.dz = c.dz;
  s.m_distance = c.m_distance;
  s.rest = c.rest;
  return s;
}

Collision* collision_from_snapshot(World& world, const CollisionSnapshot& s) {
  const WorldCollisionHost& host = world.collision_host();
  CollisionInits inits;
  if (!host.find_actor(s.aid, inits.attacker)) return nullptr;
  if (!host.find_actor(s.vid, inits.victim)) return nullptr;
  const FrameInfoMap* const aframes = host.frame_table(s.adata_id);
  if (aframes == nullptr) return nullptr;
  const FrameInfoMap* const bframes = host.frame_table(s.vdata_id);
  if (bframes == nullptr) return nullptr;
  inits.aframe = find_frame_info(*aframes, s.aframe_id);
  if (inits.aframe == nullptr) return nullptr;
  inits.bframe = find_frame_info(*bframes, s.bframe_id);
  if (inits.bframe == nullptr) return nullptr;
  {
    const double itr_index = s.itr_index;
    if (itr_index < 0 || std::trunc(itr_index) != itr_index) return nullptr;
    const std::vector<ItrInfo>& itrs = inits.aframe->itr();
    if (static_cast<std::size_t>(itr_index) >= itrs.size()) return nullptr;
    inits.itr = &itrs[static_cast<std::size_t>(itr_index)];
    const std::vector<BdyInfo>& bdys = inits.aframe->bdy();
    if (static_cast<std::size_t>(s.bdy_index) >= bdys.size()) return nullptr;
    inits.bdy = &bdys[static_cast<std::size_t>(s.bdy_index)];
  }
  inits.itr_index = s.itr_index;
  inits.bdy_index = s.bdy_index;
  Collision& ret = collision_new(world, inits);
  ret.ax = s.ax;
  ret.ay = s.ay;
  ret.az = s.az;
  ret.vx = s.vx;
  ret.vy = s.vy;
  ret.vz = s.vz;
  ret.dx = s.dx;
  ret.dy = s.dy;
  ret.dz = s.dz;
  ret.m_distance = s.m_distance;
  ret.rest = s.rest;
  return &ret;
}

void collision_clone(World& world, const Collision& src, Collision& out) {
  out = src;
  out.id = world.collision_host().new_id();
}

}
}
