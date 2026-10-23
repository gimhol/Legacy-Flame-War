#include "lfw/collision/handlers.h"

#include <variant>

#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/type_cast.h"
#include "lfw/world.h"
#include "lfw/world_collision.h"

namespace lfw {
namespace collision {
namespace {

bool missing(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

lfw::Entity* attacker_of(const Collision& c) {
  return lfw::WorldCollisionHost::entity_of_collision_a(c);
}

lfw::Entity* victim_of(const Collision& c) {
  return lfw::WorldCollisionHost::entity_of_collision_v(c);
}

lfw::WorldCollisionHost& host_of(const Collision& c) { return c.world->collision_host(); }

}

void handle_stiffness(Collision& c) {
  Entity* const a = attacker_of(c);
  Entity* const v = victim_of(c);
  const Value m = field_or(c.itr->to_value(), u"motionless");
  const Value motionless = missing(m) ? (a == nullptr ? Value() : Value(a->itr_motionless())) : m;
  if (a != nullptr) a->set_motionless(to_number(motionless));
  const Value s = field_or(c.itr->to_value(), u"shaking");
  const Value shaking = missing(s) ? Value(c.world->dataset.itr_shaking()) : s;
  if (v != nullptr) v->set_shaking(to_number(shaking));
}

void handle_body_goto(Collision& c) { handle_stiffness(c); }

void handle_super_punch_me(Collision& c) {
  Entity* const v = victim_of(c);
  if (v != nullptr) v->add_v_rest(c);
}

void handle_weapon_picked(Collision& c) {
  Entity* const a = attacker_of(c);
  Entity* const v = victim_of(c);
  if (a != nullptr && v != nullptr) a->pick(*v);
}

void handle_rest(Collision& c) {
  if (c.rest) {
    Entity* const v = victim_of(c);
    if (v != nullptr) v->add_v_rest(c);
    return;
  }
  const Value arest_v = field_or(c.itr->to_value(), u"arest");
  const double arest = truthy(arest_v) ? to_number(arest_v) : c.world->dataset.itr_arest();
  const double base =
      max(c.world->dataset.min_arest(), arest + c.world->dataset.arest_offset());
  Entity* const a = attacker_of(c);
  if (a != nullptr) a->set_arest(base);
}

void handle_itr_kind_magic_flute(Collision& c) {
  handle_rest(c);
  const std::u16string bid = u"magic_flute_to_" + c.vid;
  lfw::WorldCollisionHost& host = host_of(c);
  if (host.find_buff(bid) != nullptr) {
    host.find_buff(bid)->set_lifetime(0);
    return;
  }
  // kind 必须原样透传：MagicFlute 两个 buff 以数字 itr kind 注册，
  // 转成字符串后 factory 的严格相等查找会落空，笛子就完全不生效。
  const Value kind = field_or(c.itr->to_value(), u"kind");
  if (host.create_buff(kind, bid) == nullptr) return;
  buff::Buff* const b = host.find_buff(bid);
  if (b == nullptr) return;
  b->set_attacker_by_id(c.aid);
  b->set_victim(c.world->find_entity(c.vid));
  b->mount();
}

}
}
