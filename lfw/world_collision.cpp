#include "lfw/world_collision.h"
#include "lfw/lfw.h"

#include <algorithm>
#include <variant>

#include "lfw/base/expression.h"
#include "lfw/buff/buff_group_attack.h"
#include "lfw/collision/action_handlers.h"
#include "lfw/collision/calc_itr_velocity.h"
#include "lfw/collision/is_armor_work.h"
#include "lfw/collision/is_fall.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/js_num.h"
#include "lfw/core/js_string.h"
#include "lfw/defines/defines_data.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/entity/summary_mgr.h"
#include "lfw/loader/get_val_from_collision.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/container_help/spread_assign.h"
#include "lfw/utils/type_cast.h"
#include "lfw/world.h"

namespace lfw {
namespace {

Value index_of(const Value& holder, const std::u16string& key) {
  if (const Array* a = as_array(holder)) {
    uint32_t idx = 0;
    if (!is_array_index(key, idx)) return Value();
    if (idx >= a->size()) return Value();
    return a->at(idx);
  }
  const Object* o = as_object(holder);
  if (o == nullptr) return Value();
  const Value* p = o->get(key);
  return p == nullptr ? Value() : *p;
}

}

WorldCollisionHost::WorldCollisionHost(World& world) : _world(&world) {}

WorldCollisionHost::~WorldCollisionHost() = default;


void WorldCollisionHost::mt_mark(const std::u16string& mark) {
  MersenneTwister* const mt = _world->lfw().mt();
  if (mt != nullptr) mt->mark = mark;
}

collision::ActorHandle WorldCollisionHost::actor_of(Entity& e) const {
  return collision::ActorHandle::of(e);
}

Entity* WorldCollisionHost::entity_of_collision_a(const collision::Collision& c) {
  return const_cast<Entity*>(c.attacker.entity());
}

Entity* WorldCollisionHost::entity_of_collision_v(const collision::Collision& c) {
  return const_cast<Entity*>(c.victim.entity());
}

bool WorldCollisionHost::tester_run(const Value& tester, collision::Collision& c) {
  const std::u16string* const src = std::get_if<std::u16string>(&tester);
  if (src == nullptr) return false;
  auto it = _testers.find(*src);
  if (it == _testers.end()) {
    it = _testers.emplace(*src, Expression<collision::Collision>(
                                    *src, loader::get_val_getter_from_collision))
             .first;
  }
  return it->second.run(c);
}

Value WorldCollisionHost::tester_debug(const Value& tester) {
  (void)tester;
  return Value();
}

collision::Collision& WorldCollisionHost::acquire_collision() {
  // 复用上一拍分配过的对象，而不是每拍重新 make_unique：`collision_commit` 会把 Collision 的
  // 每个字段都重写一遍（`handlers` 只 clear 不重建），所以复用与新建完全等价，但省掉每拍
  // 几千次堆分配 + 析构 —— 100v100 实测每拍约 2400 次 commit。
  if (_collision_at < _collisions.size()) return *_collisions[_collision_at++];
  _collisions.push_back(std::make_unique<collision::Collision>());
  ++_collision_at;
  return *_collisions.back();
}

void WorldCollisionHost::reset_collisions() { _collision_at = 0; }

collision::Collision* WorldCollisionHost::collision_get(Entity& a, Entity& v) {
  return collision::collision_get(*_world, actor_of(a), actor_of(v));
}

bool WorldCollisionHost::collision_test(collision::Collision& c) {
  return collision::collision_test(c);
}

void WorldCollisionHost::handle(collision::Collision& c) {
  c.world = _world;
  collision::collisions_keeper().handle(c);
}

buff::Buff* WorldCollisionHost::find_buff(const std::u16string& id) const {
  return _world->buffs_get(id);
}

buff::Buff* WorldCollisionHost::create_buff(const Value& kind,
                                            const std::u16string& id) {
  buff::Buff* const b = _world->lfw().create_buff(kind, id);
  if (b == nullptr) return nullptr;
  _world->buffs_set(id, b);
  return b;
}

collision::Cube WorldCollisionHost::get_bounding(const collision::ActorHandle& a,
                                               const FrameInfo* frame, const ItrInfo& box) const {
  Entity* const e = const_cast<Entity*>(a.entity());
  if (e == nullptr) return collision::Cube();
  return _world->get_bounding(*e, frame, box_shape_of(box));
}

collision::Cube WorldCollisionHost::get_bounding(const collision::ActorHandle& a,
                                               const FrameInfo* frame, const BdyInfo& box) const {
  Entity* const e = const_cast<Entity*>(a.entity());
  if (e == nullptr) return collision::Cube();
  return _world->get_bounding(*e, frame, box_shape_of(box));
}

bool WorldCollisionHost::victim_get_v_rest(const collision::Collision& c) const {
  Entity* const v = entity_of_collision_v(c);
  if (v == nullptr) return false;
  return truthy(Value(v->get_v_rest(c.aid)));
}

bool WorldCollisionHost::attacker_is_ally(const collision::Collision& c) const {
  Entity* const a = entity_of_collision_a(c);
  Entity* const v = entity_of_collision_v(c);
  if (a == nullptr || v == nullptr) return false;
  return a->is_ally(*v);
}

std::u16string WorldCollisionHost::new_id() const { return _world->lfw().new_id(); }

bool WorldCollisionHost::dev() const { return _world->lfw().dev(); }

void WorldCollisionHost::log(const std::u16string& msg) const { _world->lfw().debug(msg); }

bool WorldCollisionHost::find_actor(const std::u16string& id,
                                    collision::ActorHandle& out) const {
  Entity* const e = _world->find_entity(id);
  if (e == nullptr) return false;
  out = actor_of(*e);
  return true;
}

bool WorldCollisionHost::find_object_data(const std::u16string& id, Value& out) const {
  out = _world->lfw().datas_find(Value(id));
  return truthy(out);
}

const FrameInfoMap* WorldCollisionHost::frame_table(const std::u16string& data_id) const {
  const auto it = _frame_tables.find(data_id);
  if (it != _frame_tables.end()) return &it->second;
  Value data;
  if (!find_object_data(data_id, data)) return nullptr;
  const Value frames = field_or(data, u"frames");
  return &_frame_tables.emplace(data_id, read_frame_info_map(frames)).first->second;
}

const ItrInfo* WorldCollisionHost::expanded_itr(const ItrInfo& base, const Value& prefab) {
  const std::pair<const ItrInfo*, const void*> key{&base, as_object(prefab)};
  const auto found = _expanded_itr_index.find(key);
  if (found != _expanded_itr_index.end()) return found->second;
  _expanded_itrs.push_back(std::make_unique<ItrInfo>(spread_assign(base.to_value(), prefab)));
  const ItrInfo* const info = _expanded_itrs.back().get();
  _expanded_itr_index.emplace(key, info);
  return info;
}

std::optional<double> WorldCollisionHost::priority_of(double data_type) {
  static const Value* const kMap = defines::find(u"ENTITY_PRIORITY_MAP");
  if (kMap == nullptr) return std::nullopt;
  const Value v = index_of(*kMap, number_to_string(data_type));
  if (const double* const d = std::get_if<double>(&v)) return *d;
  return std::nullopt;
}

bool WorldCollisionHost::load_handlers(collision::Collision& c) const {
  return collision::collisions_keeper().load_handlers(c);
}

void WorldCollisionHost::spark(double x, double y, double z,
                               const std::u16string& type) const {
  _world->spark(x, y, z, type);
}

void WorldCollisionHost::play_sound_global(const std::u16string& sounds, double x, double y,
                                           double z) const {
  _world->lfw().sounds_play(Value(sounds), Value(x), Value(y), Value(z));
}

Value WorldCollisionHost::hp_recoverability() const {
  return field_or(_world->world_dataset(), u"hp_recoverability");
}

void WorldCollisionHost::summary_apply_damage(Entity* a, const Value& injury, Entity* v,
                                             const Value& prev_hp) {
  if (a == nullptr || v == nullptr) return;
  summary_mgr().apply_damage(*a, injury, *v, prev_hp);
}

double WorldCollisionHost::mt_int() const {
  MersenneTwister* const mt = _world->lfw().mt();
  if (mt == nullptr) return 0.0;
  return static_cast<double>(static_cast<int32_t>(mt->next_int()));
}

Value WorldCollisionHost::mt_pick(const Value& indexes) const {
  MersenneTwister* const mt = _world->lfw().mt();
  if (mt == nullptr) return Value();
  return mt->pick_value(indexes);
}

bool WorldCollisionHost::find_data(const std::u16string& oid, Value& out) const {
  out = _world->lfw().datas_find(Value(oid));
  return truthy(out);
}

void WorldCollisionHost::broadcast(const std::u16string& msg) const {
  _world->lfw().broadcast(Value(msg));
}

void WorldCollisionHost::alert(const std::u16string& msg) const { _world->lfw().warn(msg); }

collision::ItrVelocity WorldCollisionHost::calc_velocity(collision::Collision& c) const {
  return collision::calc_itr_velocity(c);
}

bool WorldCollisionHost::is_fall(collision::Collision& c) const {
  return collision::is_fall(c);
}

bool WorldCollisionHost::is_armor_work(collision::Collision& c) const {
  return collision::is_armor_work(c);
}

void WorldCollisionHost::run_action(const std::u16string& type, const Value& action,
                                   collision::Collision& c) {
  Entity* const a = entity_of_collision_a(c);
  Entity* const v = entity_of_collision_v(c);
  if (a == nullptr || v == nullptr) return;
  collision::run_action(type, action, *a, *v, c.injury ? Value(*c.injury) : Value(NullTag{}),
                        c.real_injury ? Value(*c.real_injury) : Value(NullTag{}));
}

}
