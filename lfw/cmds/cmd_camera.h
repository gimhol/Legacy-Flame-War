#pragma once

#include "lfw/cmds/cmds.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_dist_cam_help();
void cmd_dist_cam(CMDS& ctx);
const char16_t* cmd_lock_cam_help();
void cmd_lock_cam(CMDS& ctx);

}
}
