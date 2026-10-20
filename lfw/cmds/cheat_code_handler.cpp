#include "lfw/cmds/cheat_code_handler.h"
#include "lfw/lfw.h"

#include <optional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/world.h"
#include "lfw/world_dataset.h"

namespace lfw {
namespace cmds {

namespace {

Value field_or_key(const Value& v, const std::u16string& key) {
  const Object* const o = as_object(v);
  if (o == nullptr) return Value();
  const Value* const p = o->get(key);
  return p != nullptr ? *p : Value();
}

}

void cheat_code_handler(CMDS& ctx) {
  const std::optional<std::u16string> cmd = ctx.str(0);
  if (!cmd.has_value() || !defines::is_cheat_type(*cmd)) return;
  World& world = ctx.world();
  const Value prev = world.dataset.get(*cmd);
  const std::optional<double> num = ctx.num(1);
  const double enabled = (num.has_value() && truthy(Value(*num))) ? 1.0 : 0.0;
  world.dataset.set(*cmd, Value(enabled));
  if (equals(prev, Value(enabled))) return;
  const Value* const infos = defines::find(u"Defines.CheatInfos");
  const Value cheat = infos != nullptr ? field_or_key(*infos, *cmd) : Value();
  if (!truthy(cheat)) return;
  const Value sound = field_or(cheat, u"sound");
  if (truthy(sound)) world.lfw().sounds_play_with_load(sound);
  world.lfw().cheat_changed(*cmd, enabled != 0.0);
}

}
}
