#pragma once

#include <optional>

#include "lfw/bot/state/bot_state.h"

namespace lfw {
namespace bot {

class BotState_Avoiding : public BotState_Base {
 public:
  explicit BotState_Avoiding(BotController& ctrl);
  void leave() override;
  std::optional<Value> update(double dt) override;
};

}
}
