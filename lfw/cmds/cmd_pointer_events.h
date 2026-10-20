#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "lfw/cmds/cmds.h"

namespace lfw {

class World;

namespace cmds {

struct IPointerCursor {
  double x = 0;
  double y = 0;
  bool down = false;
  bool hidden = true;
  double t = 0;
};

const char16_t* cmd_pointer_down_help();
const char16_t* cmd_pointer_move_help();
const char16_t* cmd_pointer_up_help();
const char16_t* cmd_pointer_click_help();
const char16_t* cmd_pointer_cancel_help();
const char16_t* cmd_pointer_enter_help();
const char16_t* cmd_pointer_leave_help();

void cmd_pointer_down(CMDS& ctx);
void cmd_pointer_move(CMDS& ctx);
void cmd_pointer_up(CMDS& ctx);
void cmd_pointer_click(CMDS& ctx);
void cmd_pointer_cancel(CMDS& ctx);
void cmd_pointer_enter(CMDS& ctx);
void cmd_pointer_leave(CMDS& ctx);

std::unordered_map<std::u16string, IPointerCursor>* get_pointer_cursors(World& world);
void set_local_cursor(World& world, double x, double y, std::optional<bool> hidden,
                      std::optional<bool> down);

}
}
