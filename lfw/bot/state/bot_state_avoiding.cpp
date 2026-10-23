#include "lfw/bot/state/bot_state_avoiding.h"

#include <string>
#include <variant>
#include <vector>

#include "lfw/bot/bot_controller.h"
#include "lfw/bot/state/bot_state_keys.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/state_enum.h"
#include "lfw/defines/weapon_type.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/clamp.h"
#include "lfw/utils/math/round_float.h"

namespace lfw {
namespace bot {

namespace {

constexpr double kZKeepPadNum = 3.0;
constexpr double kZKeepPadDen = 2.0;

bool frame_state_is(const Entity& e, StateEnum s) {
  return state_of_entity(e) == static_cast<double>(s);
}

}

BotState_Avoiding::BotState_Avoiding(BotController& ctrl) : BotState_Base(ctrl) {
  key_ = Value(std::u16string(bse::kAvoiding));
}

void BotState_Avoiding::leave() { ctrl_.key_up(all_game_key_names()); }

std::optional<Value> BotState_Avoiding::update(double dt) {
  (void)dt;
  BotController& c = ctrl_;
  const Entity* e = c.entity();
  if (e == nullptr) return std::nullopt;
  if (e->hp() <= 0) return next_state(bse::kDead);
  if (c.stage_is_chapter_finish()) return next_state(bse::kIdle);
  if (c.stage_is_stage_finish()) return next_state(bse::kStageEnd);
  if (c.is_leave_goto_range(*e)) return next_state(bse::kFollowing);

  const Entity* const av = this->av();
  if (handle_bot_actions(u"hba_a")) return std::nullopt;
  if (handle_defends(u"hd_a")) return std::nullopt;
  if (av == nullptr) return next_state(bse::kIdle);

  const double av_x = av->position.x;
  const double av_z = av->position.z;
  const double me_x = e->position.x;
  const double me_z = e->position.z;

  if (c.is_leave_avoid_zone(*av)) return next_state(bse::kIdle);

  const double l = c.stage_player_l();
  const double r = c.stage_player_r();

  const double av_edge_l = l + 80;
  const double av_edge_r = r - 80;
  const double av_edge_b = c.stage_near() - 35;
  const double av_edge_t = c.stage_far() + 35;

  const double av_danger_l = l + 120;
  const double av_danger_r = r - 120;
  const double av_danger_b = c.stage_near() - 60;
  const double av_danger_t = c.stage_far() + 60;

  std::u16string z_forwrd_key;
  if (av_z > av_edge_b) {
    z_forwrd_key = u"U";
  } else if (av_z < av_edge_t) {
    z_forwrd_key = u"D";
  } else if (av_z == me_z) {
    z_forwrd_key = (av_z + av->facing) > me_z ? u"U" : u"D";
  } else {
    z_forwrd_key = av_z > me_z ? u"U" : u"D";
  }

  std::u16string x_forwrd_key;
  if (av_x > av_edge_r) {
    x_forwrd_key = u"L";
  } else if (av_x < av_edge_l) {
    x_forwrd_key = u"R";
  } else if (av_x == me_x) {
    x_forwrd_key = (av_x + av->facing) > me_x ? u"L" : u"R";
  } else {
    x_forwrd_key = av_x > me_x ? u"L" : u"R";
  }

  const bool av_lying =
      state_of_entity(*av) == static_cast<double>(StateEnum::Lying);
  const bool dead_zone_only =
      av->invulnerable() == 0 && !(av_lying && av->wakeup_invuln != 0) &&
      !strict_equals(holding_base_type_of(*e), Value(static_cast<double>(WeaponEnum::Drink)));
  const bool cornered = dead_zone_only && c.cornered(*av);
  if (cornered) {
    x_forwrd_key = av_x > me_x ? u"R" : u"L";
  }

  const bool in_danger_x = (av_x > av_danger_r && me_x > av_edge_r && av_x < me_x) !=
                           (av_x < av_danger_l && me_x < av_edge_l && av_x > me_x);
  const bool in_danger_z = (av_z > av_danger_b && me_z > av_edge_b && av_z < me_z) !=
                           (av_z < av_danger_t && me_x < av_edge_t && av_z > me_z);
  const bool me_in_danger = in_danger_x && in_danger_z;

  const bool state_running = frame_state_is(*e, StateEnum::Running);
  const bool state_jump = frame_state_is(*e, StateEnum::Jump);
  bool is_jumping = me_in_danger ? wanted_jumping(u"wj_a") : random_jumping(u"rj_a");
  double jump_desire = 0;
  if (state_running && in_danger_x && !is_jumping) {
    jump_desire = defines::num(u"Defines.MAX_AI_DESIRE");
    is_jumping = true;
  }
  if (state_jump || is_jumping) {
    if (in_danger_z && me_z > av_edge_b) z_forwrd_key = u"U";
    if (in_danger_z && me_z < av_edge_t) z_forwrd_key = u"D";
  }

  const std::u16string z_backward_key = z_forwrd_key == u"D" ? u"U" : u"D";
  const std::u16string x_backward_key = x_forwrd_key == u"L" ? u"R" : u"L";
  const double avoid_out_x = c.dataset.avoid_out_x;
  const double difficulty = this->difficulty();
  const double base_desire = 100 * difficulty;
  const double ext_desire = 700 * clamp(1 - abs(me_x - av_x) / avoid_out_x, 0, 1);
  const double run_desire = round_float(base_desire + ext_desire);

  if (c.desire(u"jump_avoiding") < jump_desire) {
    c.click({u"j"});
  }
  if (c.desire(u"run_avoiding") < run_desire) {
    c.dbl_click({x_forwrd_key});
  } else {
    c.key_down({x_forwrd_key});
  }
  c.key_up({x_backward_key});
  const double rz = round(av_z - me_z);
  if (dead_zone_only && !cornered) {
    const double z_min = c.dataset.w_atk_min_z;
    const double z_max = c.dataset.w_atk_max_z;
    const double z_pad = abs(z_max - z_min);
    const double z_keep_pad = round((z_pad * kZKeepPadNum) / kZKeepPadDen);
    const bool keep_z = difficulty >= static_cast<double>(Difficulty::Difficult);
    if (keep_z && between(rz, z_min, z_max)) {
      c.key_down({z_forwrd_key});
      c.key_up({z_backward_key});
    } else if (keep_z && between(rz, z_min - z_keep_pad, z_max + z_keep_pad)) {
      c.key_up({u"U", u"D"});
    } else {
      hold_UD(rz, z_min, z_max);
    }
  } else {
    c.key_down({z_forwrd_key});
    c.key_up({z_backward_key});
  }
  return std::nullopt;
}

}
}
