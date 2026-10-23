#pragma once

#include <string>

namespace lfw::cmds {

class CMDS;

const char16_t* cmd_help_help();
void cmd_help(CMDS& ctx);

}
