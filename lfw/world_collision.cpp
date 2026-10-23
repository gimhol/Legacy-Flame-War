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
  _collisions.push_back(std::make_unique<collision::Collision>());
  return *_collisions.back();
}

void WorldCollisionHost::reset_collisions() { _collisions.clear(); }

collision::Collision* WorldCollisionHost::collision_get(Entity& a, Entity& v) {
  return collision::collision_get(*_world, actor_of(a), actor_of(v));
}

bool WorldCollisionHost::collision_test(collision::Collision& c) {
  return collision::collision_test(c);
}

void WorldCollisionHost::handle(collision::Collision& c) {
  c.world = _world;
  c.dataset = _world->world_dataset();
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
                                               const FrameInfo* frame, const Value& box) const {
  Entity* const e = const_cast<Entity*>(a.entity());
  if (e == nullptr) return collision::Cube();
  return _world->get_bounding(*e, frame, box);
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

Value WorldCollisionHost::priority_of(double data_type) {
  const Value* const map = defines::find(u"ENTITY_PRIORITY_MAP");
  if (map == nullptr) return Value();
  return index_of(*map, number_to_string(data_type));
}

bool WorldCollisionHost::load_handlers(collision::Collision& c) const {
  return collision::collisions_keeper().load_handlers(c);
}

void WorldCollisionHost::spark(const Value& x, const Value& y, const Value& z,
                               const Value& type) const {
  _world->spark(to_number(x), to_number(y), to_number(z), to_string(type));
}

void WorldCollisionHost::play_sound_global(const Value& sounds, double x, double y,
                                           double z) const {
  _world->lfw().sounds_play(sounds, Value(x), Value(y), Value(z));
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
  collision::run_action(type, action, *a, *v, c.injury, c.real_injury);
}

}
