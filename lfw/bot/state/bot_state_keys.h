#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/bot/state/bot_state.h"
#include "lfw/defines/bot_state_enum.h"
#include "lfw/defines/game_key.h"

namespace lfw {
namespace bot {

namespace bse = ::lfw::bot_state_enum;

inline const std::vector<std::u16string>& all_game_key_names() {
  return all_game_key_strs();
}

}
}
