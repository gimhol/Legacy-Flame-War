#pragma once

#include "lfw/factory.h"

namespace lfw {
namespace controller {

const ICtrlCreator* bot_controller_creator();
const ICtrlCreator* ball_controller_creator();
const ICtrlCreator* local_controller_creator();
const ICtrlCreator* invalid_controller_creator();

}
}
