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

inline std::vector<std::u16string> all_game_key_names() {
  std::vector<std::u16string> out;
  for (const char16_t* k : all_game_keys()) out.emplace_back(k);
  return out;
}

}
}
