#include "lfw/bot/bot_controller.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "lfw/base/expression.h"
#include "lfw/base/perf.h"
#include "lfw/bot/bot_env.h"
#include "lfw/bot/is_ray_hit.h"
#include "lfw/bot/state/bot_state_keys.h"
#include "lfw/core/js_num.h"
#include "lfw/core/js_string.h"
#include "lfw/core/same_ref.h"
#include "lfw/entity/entity.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/entity_enum.h"
#include "lfw/defines/game_key.h"
#include "lfw/defines/itr_kind.h"
#include "lfw/defines/oid.h"
#include "lfw/defines/state_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/helper/manhattan_xz.h"
#include "lfw/loader/get_val_from_bot_ctrl.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/clamp.h"
#include "lfw/utils/math/mersenne_twister.h"
#include "lfw/lfw.h"
#include "lfw/utils/math/round_float.h"
#include "lfw/world.h"

namespace lfw {
namespace bot {

namespace {

bool frame_state_is(const Entity& e, StateEnum s) {
  return state_of_entity(e) == static_cast<double>(s);
}

bool strict_state_is(double state, StateEnum s) {
  return state == static_cast<double>(s);
}

bool loose_state_is(double state, StateEnum s) {
  return state == static_cast<double>(s);
}

const Value& void_stage_id() {
  static const Value v = [] {
    const Value* const stage = defines::find(u"Defines.VOID_STAGE");
    return stage != nullptr ? field_or(*stage, u"id") : Value();
  }();
  return v;
}

std::u16string join_commas(const Value& keys) {
  const Array* a = as_array(keys);
  if (a == nullptr) return std::u16string();
  std::u16string out;
  for (size_t i = 0; i < a->items().size(); ++i) {
    if (i != 0) out += u",";
    out += to_string(a->items()[i]);
  }
  return out;
}

std::u16string join_queue(const std::vector<std::pair<controller::Status, std::u16string>>& queue) {
  std::u16string out;
  for (size_t i = 0; i < queue.size(); ++i) {
    if (i != 0) out += u";";
    out += to_string(Value(static_cast<double>(queue[i].first)));
    out += u",";
    out += queue[i].second;
  }
  return out;
}

bool has_length(const Value& v) {
  if (const Array* a = as_array(v)) return a->size() != 0;
  if (const std::u16string* s = std::get_if<std::u16string>(&v)) return !s->empty();
  return false;
}

std::u16string join_ids(const std::vector<const Entity*>& targets) {
  std::u16string out;
  for (size_t i = 0; i < targets.size(); ++i) {
    if (i != 0) out += u",";
    out += to_string(Value(targets[i]->id));
  }
  return out;
}

std::u16string trim_js(const std::u16string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] <= 0x20 || is_str_white_space(s[b]))) ++b;
  while (e > b && (s[e - 1] <= 0x20 || is_str_white_space(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

}

const char16_t* bot_behavior_name(BotBehavior v) {
  switch (v) {
    case BotBehavior::Stay: return u"Stay";
    case BotBehavior::Move: return u"Move";
    case BotBehavior::Follow: return u"Follow";
  }
  return u"";
}

Value BotController::dummy_default() { return Value(std::u16string(u"")); }

BotController::BotController()
    : chasings(defines::num(u"Defines.AI_MAX_CHASINGS_ENEMIES")),
      avoidings(defines::num(u"Defines.AI_MAX_AVOIDING_ENEMIES")),
      defends(defines::num(u"Defines.AI_MAX_DEFENDS_ENEMIES")),
      idle_(*this),
      chasing_(*this),
      avoiding_(*this),
      following_state_(*this),
      stage_end_(*this),
      dead_(*this) {
  goingto = Value(NullTag{});
  following = nullptr;
  watching = nullptr;
  idle_init_pending_ = true;
  fsm.add(&idle_, &chasing_, &avoiding_, &following_state_, &stage_end_, &dead_);
  set_kind(false, true);
}

void BotController::set_entity(Entity* e) {
  BaseController::set_entity(e);
  if (e != nullptr && idle_init_pending_) {
    idle_init_pending_ = false;
    fsm.reset(Value(std::u16string(bse::kIdle)));
  }
}

void BotController::reset() {
  BaseController::reset();
  chasings.clear();
  avoidings.clear();
  defends.clear();
  goingto = Value(NullTag{});
  following = nullptr;
  watching = nullptr;
  behavior = BotBehavior::Move;
  en_out_of_range = false;
  idle_min_x = kMinSafeInteger;
  idle_max_x = kMaxSafeInteger;
  idle_min_z = kMinSafeInteger;
  idle_max_z = kMaxSafeInteger;
  bot_frame = Value();
  dummy = dummy_default();
  bot_id = Value();
  bot_data_ = nullptr;
  bot_actions_ = nullptr;
  bot_src_ = nullptr;
  idle_init_pending_ = true;
}

void BotController::set_dummy(const Value& v) {
  std::vector<std::u16string> all;
  for (const char16_t* k : lfw::object_values_game_keys()) all.emplace_back(k);
  key_up(all);
  dummy = v;
}

void BotController::look_target(NearestTargets& nt, const Entity& other) {
  if (const Entity* const self = self_entity()) nt.look(*self, other);
}

void BotController::sort_targets(NearestTargets& nt) {
  if (const Entity* const self = self_entity()) nt.sort(*self);
}

double BotController::difficulty() const {
  if (lookup_cache_.active) return lookup_cache_.difficulty;
  return difficulty_uncached();
}

double BotController::difficulty_uncached() const {
  const Entity* const me = entity();
  if (me == nullptr) return 0.0;
  const Value* const void_stage = defines::find(u"Defines.VOID_STAGE");
  const Value void_id = void_stage != nullptr ? field_or(*void_stage, u"id") : Value();
  if (!strict_equals(world()->stage_value(u"id"), void_id)) {
    const std::u16string& my_team = me->team();
    for (Entity* const f : world()->puppets_values()) {
      if (f == me) continue;
      if (f->team() != my_team) continue;
      controller::BaseController* const c = f->ctrl();
      if (c != nullptr && c->is_human()) return static_cast<double>(Difficulty::Difficult);
    }
  }
  return to_number(world()->dataset.get(u"difficulty"));
}

double BotController::facing() const {
  const Entity* e = entity();
  return e != nullptr ? e->facing : 1.0;
}

const std::u16string& BotController::team() const {
  static const std::u16string kEmpty;
  const Entity* e = entity();
  return e != nullptr ? e->team() : kEmpty;
}

const Entity* BotController::en() const { return chasings.get(); }

const Entity* BotController::av() const { return avoidings.get(); }

Value BotController::bot_state() const {
  IState* s = fsm.state();
  if (s != nullptr) return s->key();
  return Value(std::u16string(bse::kIdle));
}

double BotController::atk_f_x() const {
  const Entity* e = entity();
  if (e != nullptr) {
    if (frame_state_is(*e, StateEnum::Running)) return r_atk_x();
    if (frame_state_is(*e, StateEnum::Dash)) return d_atk_max_x();
    if (frame_state_is(*e, StateEnum::Jump)) return j_atk_x();
  }
  return w_atk_f_x();
}

double BotController::atk_b_x() const {
  const Entity* e = entity();
  if (e != nullptr) {
    if (frame_state_is(*e, StateEnum::Running)) return -8;
    if (frame_state_is(*e, StateEnum::Dash)) return d_atk_min_x();
    if (frame_state_is(*e, StateEnum::Jump)) return -8;
  }
  return w_atk_b_x();
}

double BotController::w_atk_f_x() const {
  const Entity* e = entity();
  if (e == nullptr) return 0;
  const Entity* const en_e = en();
  if (en_e == nullptr) return 0;
  if (entity::is_weapon(*en_e)) return dataset.pick_weapon_f_x;
  const double wt = holding_base_type_of(*e);
  if (wt == static_cast<double>(WeaponEnum::Baseball) ||
      wt == static_cast<double>(WeaponEnum::Drink)) {
    return 800;
  }
  if (wt == static_cast<double>(WeaponEnum::Stick) ||
      wt == static_cast<double>(WeaponEnum::Knife)) {
    return 90;
  }
  if (wt == static_cast<double>(WeaponEnum::Heavy)) return 200;
  return dataset.w_atk_x;
}

double BotController::w_atk_b_x() const {
  const Entity* const en_e = en();
  if (en_e == nullptr) return 0;
  if (entity::is_weapon(*en_e)) return dataset.pick_weapon_b_x;
  return -8;
}

double BotController::r_atk_x() const {
  const Entity* e = entity();
  if (e == nullptr) return 0;
  const Entity* const en_e = en();
  if (en_e == nullptr) return 0;
  const double wt = holding_base_type_of(*e);
  if (wt == static_cast<double>(WeaponEnum::Baseball) ||
      wt == static_cast<double>(WeaponEnum::Drink)) {
    return 800;
  }
  if (wt == static_cast<double>(WeaponEnum::Heavy)) return 200;
  if (wt == static_cast<double>(WeaponEnum::Stick) ||
      wt == static_cast<double>(WeaponEnum::Knife)) {
    return 100;
  }
  return dataset.r_atk_x;
}

double BotController::d_atk_max_x() const {
  const Entity* e = entity();
  if (e == nullptr) return 0;
  if (en() == nullptr) return 0;
  const double wt = holding_base_type_of(*e);
  const double sp = e->facing * e->velocity.x * 8;
  if (wt == static_cast<double>(WeaponEnum::Baseball) ||
      wt == static_cast<double>(WeaponEnum::Drink)) {
    return sp + 100;
  }
  if (wt == static_cast<double>(WeaponEnum::Stick) ||
      wt == static_cast<double>(WeaponEnum::Knife)) {
    return 100;
  }
  return sp + dataset.d_atk_max_x;
}

double BotController::d_atk_min_x() const {
  if (en() == nullptr) return 0;
  const Entity* e = entity();
  if (e == nullptr) return 0;
  const double wt = holding_base_type_of(*e);
  if (truthy(wt)) return 0;
  return dataset.d_atk_min_x;
}

double BotController::j_atk_x() const {
  const Entity* e = entity();
  if (e == nullptr) return 0;
  if (en() == nullptr) return 0;
  const double wt = holding_base_type_of(*e);
  if (wt == static_cast<double>(WeaponEnum::Baseball) ||
      wt == static_cast<double>(WeaponEnum::Drink)) {
    return 200;
  }
  if (wt == static_cast<double>(WeaponEnum::Stick) ||
      wt == static_cast<double>(WeaponEnum::Knife)) {
    return 110;
  }
  return dataset.j_atk_x;
}

double BotController::w_atk_m_x() const {
  if (lookup_cache_.active) return lookup_cache_.w_atk_m_x;
  const Entity* e = entity();
  Value v;
  if (e != nullptr && e->holding != nullptr) {
    v = field_or(e->holding->data()->base(), u"w_atk_m_x");
  }
  return to_number(or_nullish(v, Value(dataset.w_atk_m_x)));
}

double BotController::w_atk_r_x() const {
  if (lookup_cache_.active) return lookup_cache_.w_atk_r_x;
  const Entity* e = entity();
  Value v;
  if (e != nullptr && e->holding != nullptr) {
    v = field_or(e->holding->data()->base(), u"w_atk_r_x");
  }
  return to_number(or_nullish(v, Value(dataset.w_atk_r_x)));
}

double BotController::defend_desire() const {
  const double d = difficulty() - 1;
  return round(dataset.defend_desire_base + d * dataset.defend_desire_step);
}

Value BotController::stage_value(const std::u16string& key) const {
  const Entity* e = entity();
  return e != nullptr ? world()->stage_value(key) : Value();
}

double BotController::stage_player_l() const { return to_number(stage_value(u"player_l")); }

double BotController::stage_player_r() const { return to_number(stage_value(u"player_r")); }

double BotController::stage_near() const { return to_number(stage_value(u"near")); }

double BotController::stage_far() const { return to_number(stage_value(u"far")); }

bool BotController::stage_is_stage_finish() const {
  return truthy(stage_value(u"is_stage_finish"));
}

bool BotController::stage_is_chapter_finish() const {
  return truthy(stage_value(u"is_chapter_finish"));
}

bool BotController::has_players_alive() const {
  const Entity* e = entity();
  return e != nullptr && world() != nullptr && world()->has_players_alive;
}

BotController::BoundSpan BotController::get_bound() const {
  const Entity* e = entity();
  if (e == nullptr) return BoundSpan{};
  const std::array<double, 4> b = world()->get_bound(*const_cast<Entity*>(e));
  return BoundSpan{b[0], b[1]};
}

MersenneTwister* BotController::mt() const {
  const Entity* e = entity();
  return self_entity() != nullptr ? &(*world()->lfw().mt()) : nullptr;
}

bool BotController::w_atk_too_far(const Entity& o) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double atk_r_x = w_atk_r_x();
  const double abs_dx = abs(e->position.x - o.position.x);
  return atk_r_x > 0 && atk_r_x < abs_dx;
}

bool BotController::w_atk_too_close(const Entity& o) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double atk_m_x = w_atk_m_x();
  const double abs_dx = abs(e->position.x - o.position.x);
  return atk_m_x > 0 && atk_m_x > abs_dx;
}

bool BotController::can_back_off(const Entity& o) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double x = e->position.x;
  const double en_x = o.position.x;
  const double abs_dx = abs(x - en_x);
  const BoundSpan b = lookup_cache_.active ? lookup_cache_.own_bound : get_bound();
  const double l = b.l;
  const double r = b.r;
  const double reach = en_x > x ? en_x - l : r - en_x;
  if (reach > w_atk_r_x()) return true;
  if (cornered(o)) return true;
  const double atk_m_x = w_atk_m_x();
  return atk_m_x > 0 && abs_dx < atk_m_x && reach > atk_m_x;
}

bool BotController::cornered(const Entity& o) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double x = e->position.x;
  const double en_x = o.position.x;
  const double abs_dx = abs(x - en_x);
  const double atk_m_x = w_atk_m_x();
  if (!(atk_m_x > 0) || atk_m_x <= abs_dx) return false;
  const BoundSpan b = lookup_cache_.active ? lookup_cache_.own_bound : get_bound();
  const double l = b.l;
  const double r = b.r;
  const double reach = en_x > x ? en_x - l : r - en_x;
  if (reach > atk_m_x) return false;
  return en_x > x ? x < r : x > l;
}

double BotController::should_run(const std::u16string& where, const std::optional<Pos3>& target) {
  desire(where);
  if (!target.has_value()) return 0;
  const Entity* e = entity();
  if (e == nullptr) return 0;
  const double target_x = target->x;
  const double r_x_min = dataset.r_x_min;
  double dx = round(abs(e->position.x - target_x) - r_x_min);
  bool should_run_flag = false;
  const double r_x_r = dataset.r_x_max - r_x_min;
  if (r_x_r == 0) {
    should_run_flag = desire(where + u"1") < r_x_min;
  } else {
    dx = round(clamp(dx, 0, r_x_r) / r_x_r);
    const double min_v = dataset.r_desire_min +
                         (dataset.r_desire_max - dataset.r_desire_min) * dx;
    should_run_flag = desire(where + u"#" + to_string(Value(dx))) < min_v;
  }
  if (dx < 0) should_run_flag = false;
  if (!should_run_flag) return 0;
  return e->position.x > target_x ? -1 : 1;
}

std::optional<Pos3> BotController::follow_position_uncached() const {
  if (following != nullptr) {
    return Pos3{following->position.x, following->position.y, following->position.z};
  }
  if (nullish(goingto)) return std::nullopt;
  return Pos3{to_number(field_or(goingto, u"x")), to_number(field_or(goingto, u"y")),
              to_number(field_or(goingto, u"z"))};
}

std::optional<Pos3> BotController::follow_position() const {
  if (!lookup_cache_.active) return follow_position_uncached();
  if (!lookup_cache_.follow_pos_known) {
    lookup_cache_.follow_pos_known = true;
    lookup_cache_.follow_pos = follow_position_uncached();
  }
  return lookup_cache_.follow_pos;
}

bool BotController::is_enter_goto_range(const Entity& entity) const {
  const std::optional<Pos3> pos = follow_position();
  if (!pos.has_value()) return false;
  const double x = entity.position.x;
  const double z = entity.position.z;
  const double en_x = pos->x;
  const double en_z = pos->z;
  double offset_x = 0;
  double offset_z = 0;
  if (is_following()) {
    offset_x = defines::num(u"Defines.AI_FOLLOWING_RANGE_IN_X");
    offset_z = defines::num(u"Defines.AI_FOLLOWING_RANGE_IN_Z");
  } else {
    offset_x = defines::num(u"Defines.AI_COME_RANGE_IN_X");
    offset_z = defines::num(u"Defines.AI_COME_RANGE_IN_Z");
  }
  const double bound_l = round(en_x - offset_x);
  const double bound_r = round(en_x + offset_x);
  const double bound_t = round(en_z - offset_z);
  const double bound_b = round(en_z + offset_z);
  return between(x, bound_l, bound_r) && between(z, bound_t, bound_b);
}

bool BotController::is_leave_goto_range(const Entity& entity) const {
  const std::optional<Pos3> pos = follow_position();
  if (!pos.has_value()) return false;
  const double x = entity.position.x;
  const double z = entity.position.z;
  const double en_x = pos->x;
  const double en_z = pos->z;
  double offset_x = 0;
  double offset_z = 0;
  if (is_following()) {
    offset_x = defines::num(u"Defines.AI_FOLLOWING_RANGE_OUT_X");
    offset_z = defines::num(u"Defines.AI_FOLLOWING_RANGE_OUT_Z");
  } else {
    offset_x = defines::num(u"Defines.AI_COME_RANGE_OUT_X");
    offset_z = defines::num(u"Defines.AI_COME_RANGE_OUT_Z");
  }
  const double bound_l = round(en_x - offset_x);
  const double bound_r = round(en_x + offset_x);
  const double bound_t = round(en_z - offset_z);
  const double bound_b = round(en_z + offset_z);
  return !(between(x, bound_l, bound_r) && between(z, bound_t, bound_b));
}

bool BotController::is_leave_chase_range(const Entity& target) const {
  const std::optional<Pos3> pos = follow_position();
  if (!pos.has_value()) return false;
  static const double following_out_x = defines::num(u"Defines.AI_FOLLOWING_RANGE_OUT_X");
  static const double following_out_z = defines::num(u"Defines.AI_FOLLOWING_RANGE_OUT_Z");
  static const double come_out_x = defines::num(u"Defines.AI_COME_RANGE_OUT_X");
  static const double come_out_z = defines::num(u"Defines.AI_COME_RANGE_OUT_Z");
  static const double stay_chasing_range = defines::num(u"Defines.AI_STAY_CHASING_RANGE");
  const double x = target.position.x;
  const double z = target.position.z;
  const double en_x = pos->x;
  const double en_z = pos->z;
  const bool following = is_following();
  double offset_x = following ? following_out_x : come_out_x;
  double offset_z = following ? following_out_z : come_out_z;
  offset_x += stay_chasing_range;
  offset_z += stay_chasing_range;
  const double bound_l = round(en_x - offset_x);
  const double bound_r = round(en_x + offset_x);
  const double bound_t = round(en_z - offset_z);
  const double bound_b = round(en_z + offset_z);
  return !(between(x, bound_l, bound_r) && between(z, bound_t, bound_b));
}

bool BotController::should_chase(const Entity& e_handle) {
  const Entity* e = entity();
  if (e == nullptr) return false;
  if (e->catching != nullptr) return e->catching == &e_handle;

  if (watching == &e_handle) {
    if (e_handle.mounted() == 0 || e_handle.hp() <= 0 || !is_leave_chase_range(e_handle)) {
      watching = nullptr;
    }
  }

  if (!e->mounted()) return false;
  if (e_handle.mounted() == 0) return false;
  if (e->hp() <= 0) return false;
  if (e_handle.hp() <= 0) return false;
  if (bot_ignore_of(e_handle) == 1.0) return false;
  if (e->holding == &e_handle) return false;
  if (e->catching == &e_handle) return true;
  if (e->catching != nullptr && e->catching->hp() > 0) return false;

  const double en_x = e_handle.position.x;
  const double player_l = lookup_cache_.active ? lookup_cache_.player_l : stage_player_l();
  const double player_r = lookup_cache_.active ? lookup_cache_.player_r : stage_player_r();
  if (en_x < player_l || en_x > player_r) return false;

  if (lookup_cache_.active ? lookup_cache_.self_drink
                           : holding_base_type_of(*e) == static_cast<double>(WeaponEnum::Drink)) {
    return false;
  }
  if (entity::is_fighter(e_handle) && e->team() == e_handle.team()) {
    return false;
  }

  const double e_state = state_of_entity(e_handle);
  const BoundSpan b = lookup_cache_.active ? lookup_cache_.own_bound : get_bound();
  const double l = b.l;
  const double r = b.r;
  if (!between(en_x, l, r)) return false;

  const double abs_dx = abs(e->position.x - en_x);
  if (entity::is_weapon(e_handle)) {
    const double diff = difficulty();
    if (diff == static_cast<double>(Difficulty::Easy)) return false;
    if (diff == static_cast<double>(Difficulty::Normal) &&
        base_type_of(e_handle) == static_cast<double>(WeaponEnum::Drink)) {
      return false;
    }
    if (lookup_cache_.active ? !lookup_cache_.self_free_handed : e->holding != nullptr) {
      return false;
    }
    if (is_leave_goto_range(e_handle)) return false;
    do {
      if (lookup_cache_.active
              ? lookup_cache_.stage_is_void
              : strict_equals(stage_value(u"id"), void_stage_id())) {
        break;
      }
      if (base_type_of(e_handle) != static_cast<double>(WeaponEnum::Drink)) {
        break;
      }
      if (equals(Value(e->team()), stage_value(u"team"))) break;
      if (!truthy(field_or(field_or(e_handle.data()->base(), u"drink"),
                           u"hp_h_total"))) {
        return false;
      }
      if (e->hp() >= round_float(2 * e->hp_max() / 3)) return false;
      if (!has_players_alive()) break;
      if (behavior != BotBehavior::Stay) return false;
      if (abs_dx > 100) return false;
    } while (0);

    if (loose_state_is(e_state, StateEnum::Weapon_OnGround)) return true;
    if (loose_state_is(e_state, StateEnum::HeavyWeapon_OnGround)) return true;
    return false;
  }
  if (loose_state_is(e_state, StateEnum::Lying)) return false;
  if (e_handle.invisible() != 0) return false;
  if (e_handle.invulnerable() != 0) return false;
  if (!e->is_on_ground) return true;

  if (is_leave_chase_range(e_handle)) {
    watching = &e_handle;
    return false;
  }
  if (is_avoiding()) {
    return w_atk_too_far(e_handle) || !can_back_off(e_handle);
  }
  return !w_atk_too_close(e_handle) || !can_back_off(e_handle);
}

bool BotController::is_avoiding() const {
  if (lookup_cache_.active && lookup_cache_.avoiding_known) return lookup_cache_.avoiding;
  return strict_equals(bot_state(), Value(std::u16string(bse::kAvoiding)));
}

bool BotController::is_following() const {
  if (lookup_cache_.active && lookup_cache_.following_known) return lookup_cache_.following;
  return strict_equals(bot_state(), Value(std::u16string(bse::kFollowing)));
}

void BotController::fill_lookup_cache() const {
  // `w_atk_*` 自己也会读缓存，所以先按「无缓存」的路径算好再打开
  lookup_cache_.w_atk_m_x = w_atk_m_x();
  lookup_cache_.w_atk_r_x = w_atk_r_x();
  lookup_cache_.active = true;
  lookup_cache_.avoiding = strict_equals(bot_state(), Value(std::u16string(bse::kAvoiding)));
  lookup_cache_.avoiding_known = true;
  lookup_cache_.following = strict_equals(bot_state(), Value(std::u16string(bse::kFollowing)));
  lookup_cache_.following_known = true;
  lookup_cache_.own_bound = get_bound();
  lookup_cache_.difficulty = difficulty_uncached();
  lookup_cache_.player_l = stage_player_l();
  lookup_cache_.player_r = stage_player_r();
  const Entity* const e = entity();
  lookup_cache_.self_mounted = e != nullptr && e->mounted() != 0;
  lookup_cache_.self_drink =
      e != nullptr && holding_base_type_of(*e) == static_cast<double>(WeaponEnum::Drink);
  lookup_cache_.self_free_handed = e != nullptr && e->holding == nullptr;
  lookup_cache_.stage_is_void = strict_equals(stage_value(u"id"), void_stage_id());
  lookup_cache_.follow_pos_known = false;
  lookup_cache_.follow_pos = std::nullopt;
}

void BotController::clear_lookup_cache() const {
  lookup_cache_.active = false;
  lookup_cache_.avoiding_known = false;
  lookup_cache_.following_known = false;
  lookup_cache_.follow_pos_known = false;
  lookup_cache_.follow_pos = std::nullopt;
}

bool BotController::should_avoid(const Entity& av_handle) {
  const Entity* e = entity();
  if (e == nullptr) return false;
  if (!(lookup_cache_.active ? lookup_cache_.self_mounted : e->mounted() != 0)) return false;
  if (av_handle.mounted() == 0) return false;
  if (e->hp() <= 0) return false;
  if (av_handle.hp() <= 0) return false;
  if (e->holding == &av_handle) return false;
  if (e->catching == &av_handle) return false;
  const bool avoiding = is_avoiding();
  const bool ret = avoiding ? !is_leave_avoid_zone(av_handle) : is_enter_avoid_zone(av_handle);

  const bool av_lying = strict_state_is(state_of_entity(av_handle), StateEnum::Lying);
  if (av_lying && av_handle.wakeup_invuln != 0) return ret;
  if (av_handle.invulnerable() != 0) return ret;
  if (lookup_cache_.active ? lookup_cache_.self_drink
                           : holding_base_type_of(*e) == static_cast<double>(WeaponEnum::Drink)) {
    return ret;
  }

  if (e->ground_y() != e->position.y) return false;

  if (avoiding) {
    return w_atk_r_x() > 0 && !w_atk_too_far(av_handle) && can_back_off(av_handle);
  }
  return w_atk_too_close(av_handle) && can_back_off(av_handle);
}

bool BotController::is_leave_avoid_zone(const Entity& target) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double avoid_out_x = dataset.avoid_out_x;
  const double avoid_out_z = dataset.avoid_out_z;
  const double abs_x = abs(target.position.x - e->position.x);
  const double abs_z = abs(target.position.z - e->position.z);
  return abs_x > avoid_out_x || abs_z > avoid_out_z;
}

bool BotController::is_enter_avoid_zone(const Entity& target) const {
  const Entity* e = entity();
  if (e == nullptr) return false;
  const double avoid_in_x = dataset.avoid_in_x;
  const double avoid_in_z = dataset.avoid_in_z;
  const double abs_x = abs(target.position.x - e->position.x);
  const double abs_z = abs(target.position.z - e->position.z);
  return abs_x < avoid_in_x && abs_z < avoid_in_z;
}

int BotController::should_defend(const Entity& e_handle) {
  const Entity* e = entity();
  if (e == nullptr) return 0;
  if (!(lookup_cache_.active ? lookup_cache_.self_mounted : e->mounted() != 0)) return 0;
  if (e_handle.mounted() == 0) return 0;
  if (e_handle.invisible() != 0 || e->toughness() != 0 || truthy_number(e->invulnerable())) return 0;

  const double e_state = state_of_entity(e_handle);
  do {
    if (entity::is_ball(e_handle)) break;

    if (entity::is_weapon(e_handle)) {
      if (loose_state_is(e_state, StateEnum::HeavyWeapon_InTheSky)) break;
      if (loose_state_is(e_state, StateEnum::Weapon_Throwing)) break;
      if (loose_state_is(e_state, StateEnum::Weapon_OnHand) && e_handle.bearer != nullptr &&
          !e_handle.bearer->frame->wpoint().attacking().empty()) {
        break;
      }
      return 0;
    }

    if (entity::is_fighter(e_handle)) {
      bool attacking = false;
      for (const double s : attcking_states()) {
        if (e_state == s) {
          attacking = true;
          break;
        }
      }
      if (attacking) break;
      return 0;
    }
    return 0;
  } while (0);

  BotRay ray;
  {
    const double vx = e_handle.velocity.x;
    const double vz = e_handle.velocity.z;
    static const double q = defines::num(u"Defines.DAFUALT_QUBE_LENGTH");
    ray.x = max(abs(vx), 1.0);
    ray.z = vz;
    ray.min_x = -20.0;
    ray.max_x = max(abs(10 * vx), 60.0);
    ray.min_z = -2 * q;
    ray.max_z = max(abs(10 * vz), 2 * q);
  }
  Entity* const self = self_entity();
  if (self == nullptr) return 0;
  if (!is_ray_hit(e_handle, *self, ray)) return 0;

  const std::vector<ItrInfo>& itrs = e_handle.frame->itr();
  static const double force_break_defend_value =
      defines::num(u"Defines.DEFAULT_FORCE_BREAK_DEFEND_VALUE");
  if (!itrs.empty()) {
    bool has_atk_itr_kind = false;
    bool just_a_rest = true;
    for (const ItrInfo& itr : itrs) {
      if (!itr.attacking_kind()) continue;
      has_atk_itr_kind = true;
      if (itr.bdefend() == force_break_defend_value) {
        return 2;
      }
      if (truthy_number(itr.vrest())) just_a_rest = false;
    }
    if (!has_atk_itr_kind) return 0;
    if (just_a_rest && truthy_number(e_handle.arest())) return 0;
  }
  return 1;
}

void BotController::lookup(const Entity& other) {
  const Entity* e = entity();
  if (e == nullptr) return;
  const bool self_mounted = lookup_cache_.active ? lookup_cache_.self_mounted : e->mounted() != 0;
  if (!self_mounted || other.mounted() == 0) {
    avoidings.clear();
    chasings.clear();
    defends.clear();
    if (watching == &other) watching = nullptr;
    return;
  }
  const EntityData* const od = other.data();
  if (od->is_fighter()) {
    if (e->team() == other.team()) return;
    if (should_avoid(other)) {
      look_target(avoidings, other);
      return;
    }
    const int dd = should_defend(other);
    if (dd != 0) {
      look_target(defends, other);
      return;
    }
    if (should_chase(other)) {
      look_target(chasings, other);
      return;
    }
  } else if (od->is_weapon()) {
    if (should_chase(other)) {
      look_target(chasings, other);
      return;
    }
    if (e->team() == other.team()) return;
    const int dd = should_defend(other);
    if (dd != 0) {
      look_target(defends, other);
      return;
    }
  } else if (od->is_ball()) {
    if (e->team() == other.team()) return;
    const int dd = should_defend(other);
    if (dd != 0) {
      look_target(defends, other);
      return;
    }
  } else if (od->id() == oid::kCriminal &&
             !equals(Value(e->team()), stage_value(u"team")) && !has_players_alive()) {
    look_target(chasings, other);
  } else {
    avoidings.del_ptr(&other);
    chasings.del_ptr(&other);
    defends.del_ptr(&other);
  }
}

Value BotController::guess_entity_pos(const Entity& target) const {
  const double px = target.position.x;
  const double pz = target.position.z;
  const double py = target.position.y;
  const double vx = target.velocity.x;
  const double vz = target.velocity.z;
  const double vy = target.velocity.y;
  const double state = state_of_entity(target);
  double x = 0;
  double z = 0;
  double y = 0;
  if (strict_state_is(state, StateEnum::Jump) || strict_state_is(state, StateEnum::Running)) {
    x = round_float(px + 2 * vx);
    z = round_float(pz + 1 * vz);
    y = round_float(py + 2 * vy);
  } else if (strict_state_is(state, StateEnum::Dash)) {
    x = round_float(px + 3 * vx);
    z = round_float(pz + 1 * vz);
    y = round_float(py + 1 * vy);
  } else {
    x = round_float(px + vx);
    z = round_float(pz + vz);
    y = round_float(py + vy);
  }
  Object o;
  o.set(u"x", Value(px));
  o.set(u"z", Value(pz));
  o.set(u"next_x", Value(x));
  o.set(u"next_z", Value(z));
  o.set(u"next_y", Value(y));
  return Value(std::make_shared<Object>(o));
}

double BotController::desire(const std::u16string& mark) const {
  MersenneTwister* m = mt();
  if (m == nullptr) return 0;
  m->mark = mark;
  return m->range(0, defines::num(u"Defines.MAX_AI_DESIRE"));
}

double BotController::action_desire(const std::u16string& mark) {
  double ret = desire(mark);
  for (int i = static_cast<int>(Difficulty::MAX) - static_cast<int>(difficulty()); i > 0; --i) {
    MersenneTwister* m = mt();
    if (m == nullptr) break;
    m->mark = mark;
    ret += m->range(0, ret);
  }
  return ret;
}

void BotController::check_bot() {
  const Entity* e = entity();
  if (e == nullptr) return;
  const EntityData* data = e->data();
  if (data == bot_src_) return;
  bot_src_ = data;
  const Value bot_v = field_or(data->base(), u"bot");
  const Value raw_bot_id = field_or(data->base(), u"bot_id");
  const Value bot_id_v = std::holds_alternative<std::monostate>(raw_bot_id)
                              ? Value(data->id())
                              : raw_bot_id;

  const lfw::loader::DatMgr& datas = world()->lfw().datas();
  const bot::BotData* from_base = nullptr;
  if (truthy(bot_v)) {
    from_base = datas.find_bot_data(bot_v);
    if (from_base == nullptr) from_base = datas.resolve_bot_data(bot_v);
  }
  bot_actions_ = truthy(bot_v) ? from_base : nullptr;
  if (from_base != nullptr) {
    if (from_base == bot_data_) return;
    bot_data_ = from_base;
    bot_id = Value();
    dataset = from_base->dataset();
    return;
  }
  bot_actions_ = nullptr;
  if (strict_equals(bot_id, bot_id_v)) return;
  bot_id = bot_id_v;
  if (!truthy(bot_id_v)) {
    bot_data_ = nullptr;
    dataset.reset();
    return;
  }
  bot_data_ = datas.find_bot_data(Value(to_string(bot_id_v)));
  dataset = bot_data_ != nullptr ? bot_data_->dataset() : BotDataSet::Default();
}

bool BotController::lock_when_stand_and_rest() {
  const Entity* e = entity();
  if (e == nullptr) return false;
  if (frame_state_is(*e, StateEnum::Standing) && e->resting() <= 0) {
    const double bg_width = to_number(world()->bg_value(u"width"));
    const double bg_near = to_number(world()->bg_value(u"near"));
    const_cast<Entity*>(e)->set_position(Value(bg_width / 2), Value(NullTag{}),
                                        Value((bg_near + stage_far()) / 2));
    return true;
  }
  return false;
}

Value BotController::handle_action(const std::u16string& where,
                                       const bot::BotAction& action) {
  const Value& keys_v = action.keys();
  const Value& frame = action.frame();
  if (!has_length(keys_v) && !truthy(frame)) return Value(false);

  const bool keys_is_array = as_array(keys_v) != nullptr;
  const Value mark_part = keys_is_array ? Value(join_commas(keys_v)) : frame;
  const std::u16string action_mark = u"act#" + where + u"#" + to_string(mark_part);
  const double action_desire_v = action_desire(action_mark);

  const double desire_v =
      action.has_desire() ? action.desire()
                          : action.desire_base() + action.desire_step() * (difficulty() - 1);
  if (!truthy_number(desire_v) || action_desire_v > desire_v) return Value(false);

  if (action.has_status()) {
    const Value cur = bot_state();
    bool has_state = false;
    for (const std::u16string& s : action.status()) {
      if (strict_equals(Value(s), cur)) {
        has_state = true;
        break;
      }
    }
    if (!has_state) return Value(false);
  }

  if (action.has_e_ray()) {
    const Entity* const en_e = chasings.get();
    if (en_e == nullptr || !entity::is_fighter(*en_e)) return Value(false);
    Entity* const self_e = self_entity();
    if (self_e == nullptr) return Value(false);
    bool ray_hit = false;
    for (const BotRay& r : action.e_ray()) {
      ray_hit = is_ray_hit(*self_e, *en_e, r);
      if (ray_hit) break;
    }
    if (!ray_hit) return Value(false);
  }

  if (const auto& judger = action.judger()) {
    if (!judger->run(*this)) return Value(false);
  }

  if (truthy(frame)) return frame;

  if (const Array* ks = as_array(keys_v)) {
    std::vector<Value> out;
    for (const Value& v : ks->items()) {
      const std::u16string s = to_string(v);
      if (s == u"F" || s == u"B") {
        out.push_back(Value(std::u16string(facing() > 0 ? u"R" : u"L")));
      } else {
        out.push_back(v);
      }
    }
    return Value(std::make_shared<Array>(out));
  }
  return Value(false);
}

void BotController::follow(const Entity& e_ref) {
  following = &e_ref;
  goingto = Value(NullTag{});
  behavior = BotBehavior::Move;
}

void BotController::move() {
  behavior = BotBehavior::Move;
  goingto = Value(NullTag{});
  following = nullptr;
}

void BotController::stay() {
  behavior = BotBehavior::Stay;
  if (nullish(goingto)) {
    const Entity* e = entity();
    Object o;
    o.set(u"x", Value(e != nullptr ? e->position.x : 0.0));
    o.set(u"y", Value(e != nullptr ? e->position.y : 0.0));
    o.set(u"z", Value(e != nullptr ? e->position.z : 0.0));
    goingto = Value(std::make_shared<Object>(o));
  }
  following = nullptr;
}

void BotController::come(double x, double y, double z) {
  Object o;
  o.set(u"x", Value(x));
  o.set(u"y", Value(y));
  o.set(u"z", Value(z));
  goingto = Value(std::make_shared<Object>(o));
  following = nullptr;
}

void BotController::_case(const std::u16string& mark, const std::vector<Value>& args) {
  MersenneTwister* m = mt();
  if (m == nullptr) return;
  m->mark = mark;
  m->log_case(args);
}

bool BotController::_false(const std::u16string& mark, const std::vector<Value>& args) {
  MersenneTwister* m = mt();
  if (m != nullptr) {
    m->mark = mark;
    std::vector<Value> all;
    all.push_back(Value(u"ret=false"));
    for (const Value& v : args) all.push_back(v);
    m->log_case(all);
  }
  return false;
}

bool BotController::_true(const std::u16string& mark, const std::vector<Value>& args) {
  MersenneTwister* m = mt();
  if (m != nullptr) {
    m->mark = mark;
    std::vector<Value> all;
    all.push_back(Value(u"ret=true"));
    for (const Value& v : args) all.push_back(v);
    m->log_case(all);
  }
  return true;
}

const controller::ControllerResult& BotController::update() {
  const Entity* e = entity();
  MersenneTwister* m = mt();
  if (m != nullptr && m->debugging && e != nullptr) {
    _case(u"bot_update_" + e->id + u"_" + to_string(e->name()));
  }
  check_bot();
  if (watching != nullptr && watching->mounted() == 0) {
    watching = nullptr;
  }
  if (truthy(dummy)) {
    dummy_update(dummy, *this);
  } else {
    if (e != nullptr && e->hp() > 0) {
      const bool fol_mounted = following != nullptr && following->mounted() != 0;
      const double fol_hp = following != nullptr ? following->hp() : 0.0;
      if (!fol_mounted || fol_hp <= 0) following = nullptr;
      const LookupScope cache_guard{*this};
      chasings.del([this](const Entity& t) { return !should_chase(t); });
      sort_targets(chasings);
      avoidings.del([this](const Entity& t) { return !should_avoid(t); });
      sort_targets(avoidings);
      defends.del([this](const Entity& t) { return 1 != should_defend(t); });
      sort_targets(defends);
    } else {
      following = nullptr;
      chasings.clear();
      avoidings.clear();
      defends.clear();
    }
    bot_frame = Value();
    fsm.update(1);
  }
  if (m != nullptr && m->debugging && !queue.empty()) {
    _case(u"bot_keys", {Value(join_queue(queue))});
  }
  const controller::ControllerResult& ret = BaseController::update();
  if (m != nullptr && m->debugging) {
    const std::vector<const Entity*>& a = avoidings.targets();
    const std::vector<const Entity*>& c = chasings.targets();
    const std::vector<const Entity*>& d = defends.targets();
    const std::u16string a_str = !a.empty() ? u"a=[" + join_ids(a) + u"] " : std::u16string();
    std::u16string msg = to_string(Value(to_number(Value(a_str))));
    if (!c.empty()) msg += u"c=[" + join_ids(c) + u"] ";
    if (!d.empty()) msg += u"d=[" + join_ids(d) + u"] ";
    if (!msg.empty()) {
      _case(u"bot_target", {Value(trim_js(msg))});
    }
    const Value res_frame = field_or(ret.result(), u"frame");
    if (truthy(res_frame)) _case(u"bot_nf", {field_or(res_frame, u"id")});
    if (e != nullptr) _case(u"bot_update_end_" + e->id + u"_" + to_string(e->name()));
  }
  if (truthy(bot_frame) && e != nullptr) {
    Object o;
    o.set(u"id", bot_frame);
    result.fire(Value(std::make_shared<Object>(o)), time(), u"", u"bot");
  }
  return ret;
}

double BotController::target_dist_bound() const {
  if (chasings.targets().size() < static_cast<size_t>(chasings.max()) ||
      avoidings.targets().size() < static_cast<size_t>(avoidings.max()) ||
      defends.targets().size() < static_cast<size_t>(defends.max())) {
    return std::numeric_limits<double>::infinity();
  }
  const Entity* const self_e = self_entity();
  if (self_e == nullptr) return std::numeric_limits<double>::infinity();
  const auto distance_of = [self_e](const Entity* t) {
    return helper::manhattan_xz(*self_e, *t);
  };
  double m = -1;
  for (const Entity* t : chasings.targets()) {
    const double d = distance_of(t);
    if (d > m) m = d;
  }
  for (const Entity* t : avoidings.targets()) {
    const double d = distance_of(t);
    if (d > m) m = d;
  }
  for (const Entity* t : defends.targets()) {
    const double d = distance_of(t);
    if (d > m) m = d;
  }
  return m;
}

void BotController::update_lookup(int me, const std::vector<Entity*>& entities) {
  const Entity* e = entity();
  if (e == nullptr) return;
  const Entity* const self_e = self_entity();
  if (self_e == nullptr) return;
  const LookupScope cache_guard{*this};
  const double x0 = e->position.x;
  int i1 = me - 1;
  int i2 = me + 1;
  const int len = static_cast<int>(entities.size());
  const double bound = target_dist_bound();
  const bool prune = bound != std::numeric_limits<double>::infinity();
  const double inf = std::numeric_limits<double>::infinity();
  const auto side = [&entities, len](int i) -> const Entity* {
    return (i >= 0 && i < len) ? entities[static_cast<size_t>(i)] : nullptr;
  };
  const auto dist_of = [self_e, inf](const Entity* t) {
    return t != nullptr ? helper::manhattan_xz(*self_e, *t) : inf;
  };

  const Entity* l = side(i1);
  const Entity* r = side(i2);
  double dl = dist_of(l);
  double dr = dist_of(r);
  while (l != nullptr || r != nullptr) {
    LFW_PERF_COUNT(kPerfLookupIters);
    if (prune) {
      if (l != nullptr && x0 - l->position.x > bound) {
        l = nullptr;
        dl = inf;
      }
      if (r != nullptr && r->position.x - x0 > bound) {
        r = nullptr;
        dr = inf;
      }
      if (l == nullptr && r == nullptr) break;
    }
    const Entity* picked = nullptr;
    bool adv_l = false;
    bool adv_r = false;
    if (l != nullptr && r != nullptr) {
      if (dl > dr) {
        picked = r;
        adv_r = true;
      } else {
        picked = l;
        adv_l = true;
      }
      if (l == r) {
        adv_l = true;
        adv_r = true;
      }
    } else if (l != nullptr) {
      picked = l;
      adv_l = true;
    } else {
      picked = r;
      adv_r = true;
    }
    if (picked == nullptr) break;
    if (picked->ghosted() == 0) {
      LFW_PERF_COUNT(kPerfLookupVisits);
      lookup(*picked);
    }
    if (adv_l) {
      --i1;
      l = side(i1);
      dl = dist_of(l);
    }
    if (adv_r) {
      ++i2;
      r = side(i2);
      dr = dist_of(r);
    }
  }
}

void dummy_update(const Value& dummy, BotController& self) {
  const std::u16string d = to_string(dummy);
  if (d.empty() || d == u"5") return;
  const bool is_lock_mid = d == u"1" || d == u"2" || d == u"3" || d == u"4" || d == u"6" ||
                           d == u"7" || d == u"8" || d == u"9" || d == u"10" || d == u"11" ||
                           d == u"12" || d == u"13" || d == u"14";
  if (!is_lock_mid) return;

  const Entity* e = self.entity();
  const bool h = self.lock_when_stand_and_rest();
  const bool falling = e != nullptr && frame_state_is(*e, StateEnum::Falling);
  if (d == u"1") return;
  if (d == u"2") {
    const std::vector<std::u16string> keys = {u"d"};
    if (h) {
      self.key_down(keys);
      self.key_up(keys);
    }
    return;
  }
  if (d == u"3") {
    if (falling) {
      self.key_down({u"j"});
      self.key_up({u"j"});
    }
    return;
  }
  if (d == u"4") {
    if (h || falling) {
      self.key_down({u"j"});
      self.key_up({u"j"});
    } else {
      self.end({u"j"});
    }
    return;
  }
  if (!h) return;
  std::vector<std::u16string> keys;
  if (d == u"6") keys = {u"d", u"U", u"a"};
  else if (d == u"7") keys = {u"d", u"U", u"j"};
  else if (d == u"8") keys = {u"d", u"D", u"a"};
  else if (d == u"9") keys = {u"d", u"D", u"j"};
  else if (d == u"10") keys = {u"d", u"L", u"a"};
  else if (d == u"11") keys = {u"d", u"L", u"j"};
  else if (d == u"12") keys = {u"d", u"R", u"a"};
  else if (d == u"13") keys = {u"d", u"L", u"j"};
  else if (d == u"14") keys = {u"d", u"j", u"a"};
  else return;
  self.key_down(keys);
  self.key_up(keys);
}

}
}
