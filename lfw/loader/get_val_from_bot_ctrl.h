#pragma once

#include <string>

#include "lfw/base/expression.h"
#include "lfw/bot/bot_controller.h"

namespace lfw {
namespace loader {

ValGetter<bot::BotController> get_val_from_bot_ctrl(const std::u16string& word);

}
}
