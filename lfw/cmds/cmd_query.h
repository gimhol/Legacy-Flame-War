#pragma once

#include "lfw/cmds/cmds.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_list_help();
void cmd_list(CMDS& ctx);
const char16_t* cmd_entities_help();
void cmd_entities(CMDS& ctx);
const char16_t* cmd_info_help();
void cmd_info(CMDS& ctx);
const char16_t* cmd_ui_help();
void cmd_ui(CMDS& ctx);

}
}
