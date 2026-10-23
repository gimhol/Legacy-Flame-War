#include "lfw/cmds/cmd_help.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/cmds/cmds.h"
#include "lfw/core/js_string.h"
#include "lfw/lfw.h"
#include "lfw/utils/string_help.h"
#include "lfw/world.h"

namespace lfw::cmds {

namespace {

std::u16string upper(const std::u16string& s) {
  std::u16string out = s;
  for (char16_t& c : out) {
    if (c >= u'a' && c <= u'z') c = static_cast<char16_t>(c - u'a' + u'A');
  }
  return out;
}

constexpr size_t kColumns = 4;
constexpr size_t kColumnWidth = 18;

}

const char16_t* cmd_help_help() {
  return uR"(Usage: HELP [command]

List every command / show one command's full help
列出所有可用命令；给了命令名就显示该命令的完整帮助)";
}

void cmd_help(CMDS& ctx) {
  const std::vector<CMDS::Entry> all = CMDS::entries();
  if (const std::optional<std::u16string> which = ctx.str(1); which.has_value() && !which->empty()) {
    const std::u16string want = to_lower_case(*which);
    for (const CMDS::Entry& entry : all) {
      if (entry.first != want) continue;
      ctx.world().lfw().log(entry.second);
      return;
    }
    ctx.world().lfw().warn(u"HELP: unknown command \"" + *which + u"\"（用 `help` 看全部）");
    return;
  }

  std::u16string out = number_to_string(static_cast<double>(all.size())) + u" commands:";
  std::u16string row;
  size_t in_row = 0;
  for (const CMDS::Entry& entry : all) {
    std::u16string name = upper(entry.first);
    if (name.size() < kColumnWidth) name.append(kColumnWidth - name.size(), u' ');
    row += name;
    if (++in_row == kColumns) {
      out += u"\n  " + row;
      row.clear();
      in_row = 0;
    }
  }
  if (!row.empty()) out += u"\n  " + row;
  out += u"\n（`help <command>` 看某条命令的完整帮助）";
  ctx.world().lfw().log(out);
}

}
