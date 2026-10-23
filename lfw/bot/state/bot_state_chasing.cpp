#include "lfw/bot/state/bot_state_chasing.h"

#include <string>
#include <variant>
#include <vector>

#include "lfw/bot/bot_controller.h"
#include "lfw/bot/state/bot_state_keys.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/entity_enum.h"
#include "lfw/defines/state_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"

namespace lfw {
namespace bot {

namespace {

bool frame_state_is(const Entity& e, StateEnum s) {
  return strict_equals(state_of_entity(e), Value(static_cast<double>(s)));
}

}

BotState_Chasing::BotState_Chasing(BotController& ctrl) : BotState_Base(ctrl) {
  key_ = Value(std::u16string(bse::kChasing));
}

std::optional<Value> BotState_Chasing::update(double dt) {
  (void)dt;
  BotController& c = ctrl_;
  const Entity* e = c.entity();
  if (e == nullptr) return std::nullopt;
  if (e->hp() <= 0) return next_state(bse::kDead);
  if (c.stage_is_chapter_finish()) return next_state(bse::kIdle);
  if (c.stage_is_stage_finish()) return next_state(bse::kStageEnd);

  const Entity* const en = c.chasings.get();
  if (en != nullptr && c.is_leave_chase_range(*en)) return next_state(bse::kFollowing);
  if (c.is_leave_goto_range(*e)) return next_state(bse::kFollowing);

  if (handle_bot_actions(u"hba_c")) return std::nullopt;
  if (handle_defends(u"hd_c1")) return std::nullopt;
  handle_block();

  if (en == nullptr) return next_state(bse::kIdle);

  const double me_facing = e->facing;
  const double my_x = e->position.x;
  const double my_z = e->position.z;
  const Value guess = c.guess_entity_pos(*en);
  const double en_x = to_number(field_or(guess, u"next_x"));
  const double en_z = to_number(field_or(guess, u"next_z"));

  const double rz = round(en_z - my_z);
  const double rx = round(me_facing * (en_x - my_x));
  const double absx = round(abs(my_x - en_x));

  if (absx > defines::num(u"Defines.AI_STAY_CHASING_RANGE") &&
      (c.behavior == BotBehavior::Stay || c.behavior == BotBehavior::Follow)) {
    c.en_out_of_range = true;
    return next_state(bse::kFollowing);
  }

  const double abs_dz = round_float(abs(my_z - en_z));
  const double atk_b_x = c.atk_b_x();
  const double atk_f_x = c.atk_f_x();
  const bool x_ok = between(rx, atk_b_x, atk_f_x);
  const bool z_ok = between(rz, c.dataset.w_atk_min_z,
                            c.dataset.w_atk_max_z);

  if ((!x_ok || !z_ok) &&
      equals(Value(en->data()->type()),
             Value(static_cast<double>(EntityEnum::Fighter)))) {
    random_jumping(u"rj_c");
  }

  const Value wt = holding_base_type_of(*e);

  const Value stage_team = c.stage_value(u"team");
  const double player_l = c.stage_player_l();
  const double player_r = c.stage_player_r();
  c.en_out_of_range = !equals(Value(e->team()), stage_team) &&
                      (en_x < player_l - 80 || en_x > player_r + 80);

  const char16_t* gk_f = me_facing > 0 ? u"R" : u"L";
  const Value state = state_of_entity(*e);
  if (strict_equals(state, Value(static_cast<double>(StateEnum::Running)))) {
    return update_running();
  }
  if (strict_equals(state, Value(static_cast<double>(StateEnum::Dash)))) {
    return update_dash();
  }
  if (strict_equals(state, Value(static_cast<double>(StateEnum::Jump)))) {
    return update_jump();
  }
  if (strict_equals(state, Value(static_cast<double>(StateEnum::Catching)))) {
    if (e->catching != nullptr) c.click({u"a"});
  } else if (strict_equals(state, Value(static_cast<double>(StateEnum::Standing))) ||
             strict_equals(state, Value(static_cast<double>(StateEnum::Walking)))) {
    if (!c.en_out_of_range) {
      const double r_desire =
          c.should_run(u"sr_c", Pos3{en->position.x, en->position.y, en->position.z});
      if (r_desire == 0) {
      } else {
        if (r_desire > 0) {
          c.db_hit({u"R"});
          c.end({u"R"});
          c.key_up({u"L"});
        } else {
          c.db_hit({u"L"});
          c.end({u"L"});
          c.key_up({u"R"});
        }
        return std::nullopt;
      }
    } else if (truthy(wt) && between(abs_dz, 0, 30)) {
      if (equals(wt, Value(static_cast<double>(WeaponEnum::Knife)))) {
        if (c.desire(u"rtwd") < 400) {
          c.key_down({gk_f});
          c.click({u"a"});
          return std::nullopt;
        }
      }
    }
  } else {
    c.key_up(all_game_key_names());
  }

  if (x_ok && z_ok) c.click({u"a"});
  hold_UD(rz, c.dataset.w_atk_min_z, c.dataset.w_atk_max_z);
  hold_LR(rx, atk_b_x, atk_f_x);
  if (equals(stage_team, Value(e->team()))) {
    if (my_x < player_l) c.click({u"R"});
    if (my_x > player_r) c.click({u"L"});
  }
  return std::nullopt;
}

std::optional<Value> BotState_Chasing::update_dash() {
  BotController& c = ctrl_;
  const Entity* e = c.entity();
  if (e == nullptr) return std::nullopt;
  const Entity* const en = this->en();
  if (en == nullptr) return std::nullopt;

  const double my_x = e->position.x;
  const double my_z = e->position.z;
  const double en_x = en->position.x;
  const double en_z = en->position.z;
  const double me_facing = e->facing;
  const double rx = round(me_facing * (en_x - my_x));
  const double rz = round(en_z - my_z);
  const bool x_ok = rx >= c.d_atk_min_x() && rx <= c.d_atk_max_x();
  const bool z_ok = rz >= c.dataset.d_atk_min_z &&
                    rx <= c.dataset.d_atk_max_z;
  if (x_ok && z_ok) {
    c.click({u"a"});
  }
  return std::nullopt;
}

std::optional<Value> BotState_Chasing::update_running() {
  BotController& c = ctrl_;
  const Entity* e = c.entity();
  if (e == nullptr) return std::nullopt;
  const Entity* const en = this->en();
  if (en == nullptr) return std::nullopt;
  const Value wt = holding_base_type_of(*e);

  const double me_facing = e->facing;
  const double my_x = e->position.x;
  const double my_z = e->position.z;
  const double en_x = en->position.x;
  const double en_z = en->position.z;

  const char16_t* gk_f = me_facing > 0 ? u"R" : u"L";
  const char16_t* gk_b = me_facing > 0 ? u"L" : u"R";

  c.key_up({gk_f});

  const double rz = round(en_z - my_z);
  const double rx = round(me_facing * (en_x - my_x));

  const bool x_ok = between(rx, c.w_atk_b_x(), c.w_atk_f_x());
  const bool z_ok = between(rz, c.dataset.r_atk_min_z,
                            c.dataset.r_atk_max_z);

  if (x_ok && z_ok) {
    if (entity::is_weapon(*en)) {
      c.click({gk_b});
    } else if (entity::is_fighter(*en)) {
      if (c.desire(u"chasing_1") < c.dataset.r_atk_desire) {
        c.click({u"a"});
      }
    } else {
      c.click({u"a"});
    }
    return std::nullopt;
  }

  hold_UD(rz, c.dataset.r_atk_min_z, c.dataset.r_atk_max_z);

  if (!x_ok) {
    if ((my_x > en_x && me_facing > 0) || (my_x < en_x && me_facing < 0) ||
        c.desire(u"chasing_stop_running_1") < c.dataset.r_stop_desire) {
      c.click({gk_b});
      return std::nullopt;
    }
  }

  if (truthy(wt)) {
    const double d = c.desire(u"rtwd_1");
    const bool t_ok = (equals(wt, Value(static_cast<double>(WeaponEnum::Knife))) && d < 400) ||
                      (equals(wt, Value(static_cast<double>(WeaponEnum::Stick))) && d < 100) ||
                      (equals(wt, Value(static_cast<double>(WeaponEnum::Drink))) && d < 50);
    if (t_ok) {
      c.click({gk_f});
      c.click({u"a"});
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<Value> BotState_Chasing::update_jump() {
  BotController& c = ctrl_;
  const Entity* e = c.entity();
  if (e == nullptr) return std::nullopt;
  const Entity* const en = this->en();
  if (en == nullptr) return std::nullopt;
  const Value wt = holding_base_type_of(*e);

  const double me_facing = e->facing;
  const double my_x = e->position.x;
  const double my_z = e->position.z;
  const double my_y = e->position.y;
  const double en_x = en->position.x;
  const double en_z = en->position.z;
  const double en_y = en->position.y;

  const double rz = round(en_z - my_z);
  const double rx = round(me_facing * (en_x - my_x));
  const double ry = round(en_y - my_y);

  if (e->is_on_ground) {
    hold_UD(rz, -30, 30);
    hold_LR(rx, -60, 60);
    return std::nullopt;
  }

  const char16_t* gk_f = me_facing > 0 ? u"R" : u"L";
  if (truthy(wt)) {
    const double d = c.desire(u"jtw");
    const bool t_ok = (equals(wt, Value(static_cast<double>(WeaponEnum::Knife))) && d < 400) ||
                      (equals(wt, Value(static_cast<double>(WeaponEnum::Stick))) && d < 100) ||
                      (equals(wt, Value(static_cast<double>(WeaponEnum::Drink))) && d < 50);
    if (t_ok) {
      c.click({gk_f});
      c.click({u"a"});
    }
  }

  const bool x_ok = between(rx, 0, c.j_atk_x());
  const bool z_ok = between(rz, c.dataset.j_atk_min_z,
                            c.dataset.j_atk_max_z);
  const bool y_ok = between(ry, c.dataset.j_atk_min_y,
                            c.dataset.j_atk_max_y);
  if (x_ok && z_ok && y_ok) {
    c.click({u"a"});
    return std::nullopt;
  }

  c.key_up({u"R", u"L", u"U", u"D"});
  return std::nullopt;
}

}
}
