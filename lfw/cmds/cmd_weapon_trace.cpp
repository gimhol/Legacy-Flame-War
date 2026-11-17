#include "lfw/cmds/cmd_weapon_trace.h"

#include <string>
#include <vector>

#include "lfw/cmds/cmds.h"
#include "lfw/core/js_string.h"
#include "lfw/debug/weapon_release_trace.h"
#include "lfw/lfw.h"
#include "lfw/world.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_weapon_trace_help() {
  return u"武器出手诊断: WEAPON_TRACE on|off 开关实时打印; WEAPON_TRACE dump 打印最近记录; WEAPON_TRACE clear 清空";
}

void cmd_weapon_trace(CMDS& ctx) {
  LFW& lfw = ctx.world().lfw();
  std::u16string action = u"dump";
  if (ctx.words().size() > 1) action = ctx.words()[1];
  if (action == u"on") {
    debug::set_weapon_release_trace_enabled(true);
    lfw.log(u"[weapon_release] trace: on");
    return;
  }
  if (action == u"off") {
    debug::set_weapon_release_trace_enabled(false);
    lfw.log(u"[weapon_release] trace: off");
    return;
  }
  if (action == u"clear") {
    debug::clear_weapon_release_traces();
    lfw.log(u"[weapon_release] cleared");
    return;
  }
  const std::vector<std::u16string> lines = debug::dump_weapon_release_traces();
  lfw.log(u"[weapon_release] enabled=" +
          std::u16string(debug::weapon_release_trace_enabled() ? u"1" : u"0") +
          u" count=" + number_to_string(static_cast<double>(lines.size())));
  for (const std::u16string& line : lines) lfw.log(line);
}

}
}
