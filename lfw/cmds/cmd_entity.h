#pragma once

#include "lfw/cmds/cmds.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_entity_help();
void cmd_entity(CMDS& ctx);
const char16_t* cmd_despawn_help();
void cmd_despawn(CMDS& ctx);
const char16_t* cmd_del_puppet_help();
void cmd_del_puppet(CMDS& ctx);

}
}
