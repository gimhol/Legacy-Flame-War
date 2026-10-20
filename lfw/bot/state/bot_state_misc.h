#pragma once

#include <optional>

#include "lfw/bot/state/bot_state.h"

namespace lfw {
namespace bot {

class BotState_Following : public BotState_Base {
 public:
  explicit BotState_Following(BotController& ctrl);
  void enter() override;
  std::optional<Value> update(double dt) override;
};

class BotState_StageEnd : public BotState_Base {
 public:
  explicit BotState_StageEnd(BotController& ctrl);
  void enter() override;
  void leave() override;
  std::optional<Value> update(double dt) override;
};

class BotState_Dead : public BotState_Base {
 public:
  explicit BotState_Dead(BotController& ctrl);
  void enter() override;
  std::optional<Value> update(double dt) override;
};

}
}
