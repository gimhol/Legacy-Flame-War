#pragma once

#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/bot/bot_dataset.h"
#include "lfw/bot/nearest_targets.h"
#include "lfw/bot/state/bot_state_avoiding.h"
#include "lfw/bot/state/bot_state_chasing.h"
#include "lfw/bot/state/bot_state_idle.h"
#include "lfw/bot/state/bot_state_misc.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"

namespace lfw {
namespace bot {

enum class BotBehavior : int {
  Stay = 0,
  Move = 1,
  Follow = 2,
};

const char16_t* bot_behavior_name(BotBehavior v);

class BotController : public controller::BaseController {
 public:
  static constexpr const char* TAG = "BotController";

  BotController();

  void set_entity(Entity* e) override;
  void reset() override;
  const controller::ControllerResult& update() override;
  void update_lookup(int me, const std::vector<Entity*>& entities);

  FSM fsm;
  NearestTargets chasings;
  NearestTargets avoidings;
  NearestTargets defends;
  Value dummy{dummy_default()};
  BotBehavior behavior = BotBehavior::Move;
  Value goingto;
  const Entity* following = nullptr;
  const Entity* watching = nullptr;
  bool en_out_of_range = false;
  double idle_min_x = -9007199254740991.0;
  double idle_max_x = 9007199254740991.0;
  double idle_min_z = -9007199254740991.0;
  double idle_max_z = 9007199254740991.0;
  Value bot_id;
  Value bot;
  BotDataSet dataset;
  Value bot_frame;

  double difficulty() const;
  double facing() const;
  const std::u16string& team() const;
  Value en() const;
  Value av() const;
  Value bot_state() const;
  double atk_f_x() const;
  double atk_b_x() const;
  double w_atk_f_x() const;
  double w_atk_b_x() const;
  double r_atk_x() const;
  double d_atk_max_x() const;
  double d_atk_min_x() const;
  double j_atk_x() const;
  double w_atk_m_x() const;
  double w_atk_r_x() const;
  double defend_desire() const;

  Value stage_value(const std::u16string& key) const;
  double stage_player_l() const;
  double stage_player_r() const;
  double stage_near() const;
  double stage_far() const;
  bool stage_is_stage_finish() const;
  bool stage_is_chapter_finish() const;
  bool has_players_alive() const;
  std::vector<double> get_bound() const;
  MersenneTwister* mt() const;

  bool w_atk_too_far(const Value& o_ref) const;
  bool w_atk_too_close(const Value& o_ref) const;
  bool can_back_off(const Value& o_ref) const;
  bool cornered(const Value& o_ref) const;
  double should_run(const std::u16string& where, const Value& target);
  bool is_enter_goto_range(const Value& entity_ref) const;
  bool is_leave_goto_range(const Value& entity_ref) const;
  bool is_leave_chase_range(const Value& target_ref) const;
  bool should_chase(const Entity& e_ref);
  bool should_avoid(const Entity& av_ref);
  int should_defend(const Entity& e_ref);
  void lookup(const Entity& other);
  Value guess_entity_pos(const Value& entity_ref) const;
  double desire(const std::u16string& mark) const;
  double action_desire(const std::u16string& mark);
  void check_bot();
  bool lock_when_stand_and_rest();
  Value handle_action(const std::u16string& where, const Value& action);
  void follow(const Entity& e_ref);
  void move();
  void stay();
  void come(double x, double y, double z);
  void _case(const std::u16string& mark, const std::vector<Value>& args = {});
  bool _false(const std::u16string& mark, const std::vector<Value>& args = {});
  bool _true(const std::u16string& mark, const std::vector<Value>& args = {});
  bool is_leave_avoid_zone(const Value& target_ref) const;
  bool is_enter_avoid_zone(const Value& target_ref) const;
  Value self_ref() const;
  Entity* self_entity() const { return entity(); }
  void look_target(NearestTargets& nt, const Entity& other);
  void sort_targets(NearestTargets& nt);

  const Value& dummy_value() const { return dummy; }
  void set_dummy(const Value& v);

  static Value dummy_default();

 private:
  double target_dist_bound() const;

  bool idle_init_pending_ = false;

  BotState_Idle idle_;
  BotState_Chasing chasing_;
  BotState_Avoiding avoiding_;
  BotState_Following following_state_;
  BotState_StageEnd stage_end_;
  BotState_Dead dead_;
};

void dummy_update(const Value& dummy, BotController& self);

}
}
