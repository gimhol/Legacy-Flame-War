#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/bot/bot_data.h"
#include "lfw/bot/bot_dataset.h"
#include "lfw/bot/nearest_targets.h"
#include "lfw/bot/state/bot_state_avoiding.h"
#include "lfw/bot/state/bot_state_chasing.h"
#include "lfw/bot/state/bot_state_idle.h"
#include "lfw/bot/state/bot_state_misc.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/js_num.h"
#include "lfw/core/value.h"

namespace lfw {
namespace bot {

enum class BotBehavior : int {
  Stay = 0,
  Move = 1,
  Follow = 2,
};

/** 一个位置。Bot 只用来把「跟随目标/前往点」传进距离判断。 */
struct Pos3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
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
  double idle_min_x = kMinSafeInteger;
  double idle_max_x = kMaxSafeInteger;
  double idle_min_z = kMinSafeInteger;
  double idle_max_z = kMaxSafeInteger;
  Value bot_id;
  const bot::BotData* bot_data_ = nullptr;
  const bot::BotData* bot_actions_ = nullptr;
  // 运行期自造的 bot 数据（不在 DatMgr 缓存里）用它兜底
  std::optional<bot::BotData> bot_data_owned_;
  BotDataSet dataset;

  /** `check_bot()` 解析出的当前 bot 数据（含 bot_id 兜底路径）。 */
  const bot::BotData* bot_data() const { return bot_data_; }
  /** 只认“实体数据的 `base.bot` 字段”解析出来的那份：`handle_bot_actions` 沿用这个门禁。 */
  const bot::BotData* bot_actions() const { return bot_actions_; }
  Value bot_frame;

  double difficulty() const;
  double facing() const;
  const std::u16string& team() const;
  const Entity* en() const;
  const Entity* av() const;
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

  bool w_atk_too_far(const Entity& o) const;
  bool w_atk_too_close(const Entity& o) const;
  bool can_back_off(const Entity& o) const;
  bool cornered(const Entity& o) const;
  double should_run(const std::u16string& where, const std::optional<Pos3>& target);
  bool is_enter_goto_range(const Entity& entity) const;
  bool is_leave_goto_range(const Entity& entity) const;
  bool is_leave_chase_range(const Entity& target) const;
  bool should_chase(const Entity& e_ref);
  bool should_avoid(const Entity& av_ref);
  int should_defend(const Entity& e_ref);
  void lookup(const Entity& other);
  std::optional<Pos3> follow_position() const;
  Value guess_entity_pos(const Entity& target) const;
  double desire(const std::u16string& mark) const;
  double action_desire(const std::u16string& mark);
  void check_bot();
  bool lock_when_stand_and_rest();
  Value handle_action(const std::u16string& where, const bot::BotAction& action);
  void follow(const Entity& e_ref);
  void move();
  void stay();
  void come(double x, double y, double z);
  void _case(const std::u16string& mark, const std::vector<Value>& args = {});
  bool _false(const std::u16string& mark, const std::vector<Value>& args = {});
  bool _true(const std::u16string& mark, const std::vector<Value>& args = {});
  bool is_leave_avoid_zone(const Entity& target) const;
  bool is_enter_avoid_zone(const Entity& target) const;
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
