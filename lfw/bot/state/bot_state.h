#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/bot/bot_env.h"
#include "lfw/core/value.h"

namespace lfw {
namespace bot {

class BotController;

inline std::optional<Value> next_state(const char16_t* key) {
  return std::optional<Value>(Value(std::u16string(key)));
}

class BotState_Base : public IState {
 public:
  explicit BotState_Base(BotController& ctrl) : ctrl_(ctrl) {}

  Value key() const override { return key_; }

  BotController& ctrl() const { return ctrl_; }

  double difficulty() const;
  const Entity* en() const;
  const Entity* av() const;
  const Entity* closest(const std::vector<const Entity*>& list) const;

  bool wanted_jumping(const std::u16string& where);
  bool random_jumping(const std::u16string& where);
  bool handle_defends(const std::u16string& mark);
  bool handle_block();
  bool handle_bot_actions(const std::u16string& where);
  void hold_UD(double rz, double min_z, double max_z);
  void hold_LR(double rx, double min_x, double max_x);

 protected:
  BotController& ctrl_;
  Value key_;
};

}
}
