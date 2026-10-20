#pragma once

#include "lfw/cmds/cmds.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_kill_help();
void cmd_kill(CMDS& ctx);
const char16_t* cmd_kill_boss_help();
void cmd_kill_boss(CMDS& ctx);
const char16_t* cmd_kill_enemies_help();
void cmd_kill_enemies(CMDS& ctx);
const char16_t* cmd_kill_others_help();
void cmd_kill_others(CMDS& ctx);
const char16_t* cmd_kill_soliders_help();
void cmd_kill_soliders(CMDS& ctx);

}
}
