#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lfw/bot/bot_dataset.h"
#include "lfw/core/value.h"

namespace lfw {

template <typename Ctx>
class Expression;

namespace bot {

class BotController;

/** `IBotRay`：针对敌人的 XZ 射线检测。 */
struct BotRay {
  double x = 0;
  double z = 0;
  double min_x = 0;
  double max_x = 10000.0;
  double min_z = 0;
  double max_z = 10000.0;
  double max_d = 0;
  bool has_max_d = false;
  bool reverse = false;
};

/** `IBotAction`：BOT 的一个动作。判定式在加载期就编译成 `judger`。 */
class BotAction {
 public:
  BotAction() = default;
  explicit BotAction(Value raw);

  const std::u16string& action_id() const { return _action_id; }
  bool has_desire() const { return _has_desire; }
  double desire() const { return _desire; }
  double desire_base() const { return _desire_base; }
  double desire_step() const { return _desire_step; }
  const std::vector<BotRay>& e_ray() const { return _e_ray; }
  bool has_e_ray() const { return _has_e_ray; }
  bool has_status() const { return _has_status; }
  const std::vector<std::u16string>& status() const { return _status; }
  const Value& keys() const { return _keys; }
  const Value& frame() const { return _frame; }
  const std::shared_ptr<Expression<BotController>>& judger() const { return _judger; }

  /** JS 边界（快照 / 表达式）专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _action_id;
  bool _has_desire = false;
  double _desire = 0;
  double _desire_base = 0;
  double _desire_step = 0;
  bool _has_e_ray = false;
  std::vector<BotRay> _e_ray;
  bool _has_status = false;
  std::vector<std::u16string> _status;
  Value _keys;
  Value _frame;
  std::shared_ptr<Expression<BotController>> _judger;
};

/** `IBotData`：一个 BOT 的完整数据（动作表 + 帧/状态索引 + dataset）。 */
class BotData {
 public:
  BotData() = default;
  explicit BotData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& oid() const { return _oid; }
  const BotAction* action(const std::u16string& action_id) const;
  const std::vector<std::u16string>* actions_of_state(const std::u16string& state) const;
  const std::vector<std::u16string>* actions_of_frame(const std::u16string& frame_id) const;
  const BotDataSet& dataset() const { return _dataset; }
  std::size_t action_count() const { return _actions.size(); }

  /** JS 边界（快照 / 表达式）专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _oid;
  std::map<std::u16string, BotAction> _actions;
  std::map<std::u16string, std::vector<std::u16string>> _states;
  std::map<std::u16string, std::vector<std::u16string>> _frames;
  BotDataSet _dataset;
};

}
}
