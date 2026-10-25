#include "lfw/cmds/cmd_entity.h"
#include "lfw/lfw.h"

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/controller/base_controller.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/frame_id.h"
#include "lfw/entity/entity.h"
#include "lfw/state/entity_states.h"
#include "lfw/state/state_base.h"
#include "lfw/world.h"

namespace lfw {
namespace cmds {

namespace {

constexpr size_t kKeyWidth = 10;

std::u16string num(double v) { return number_to_string(v); }

std::u16string vec3(const Vector3& p) {
  return u"(" + num(p.x) + u"," + num(p.y) + u"," + num(p.z) + u")";
}

bool usable(const std::u16string& text) { return !text.empty() && text != u"undefined"; }

/** 引用类字段的显示：`#101` / `-` */
std::u16string entity_ref(const Entity* e) { return e != nullptr ? u"#" + e->id : std::u16string(u"-"); }

std::u16string ctrl_name(const Entity& e) {
  const controller::BaseController* const ctrl = e.ctrl();
  if (ctrl == nullptr) return u"none";
  if (ctrl->is_human()) return u"player";
  return ctrl->is_ball_ctrl() ? u"ball" : u"bot";
}

/** `#101` 与 `101` 都认（`ENTITIES` 打出来的就是带 `#` 的）；残影 / 傀儡不在 `entity_map` 里，再兜一圈 */
Entity* find_entity_any(World& world, const std::u16string& raw) {
  const std::u16string id = !raw.empty() && raw[0] == u'#' ? raw.substr(1) : raw;
  if (id.empty()) return nullptr;
  if (Entity* const e = world.find_entity(id); e != nullptr) return e;
  for (Entity* const e : world.ghosts) {
    if (e != nullptr && e->id == id) return e;
  }
  for (const std::pair<std::u16string, Entity*>& kv : world.puppets) {
    if (kv.second != nullptr && kv.second->id == id) return kv.second;
  }
  return nullptr;
}

/** 命令行的值 → 数据侧的值：能当数字就当数字，`true`/`false` 当 0/1，`null`/`none`（或空）当空 */
Value value_of(const std::u16string& text) {
  if (text.empty() || text == u"null" || text == u"none") return Value();
  if (text == u"true") return Value(1.0);
  if (text == u"false") return Value(0.0);
  const double n = string_to_number(text);
  return std::isnan(n) ? Value(text) : Value(n);
}

std::vector<double> parse_nums(const std::u16string& text) {
  std::vector<double> out;
  size_t start = 0;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i != text.size() && text[i] != u',') continue;
    out.push_back(string_to_number(text.substr(start, i - start)));
    start = i + 1;
  }
  return out;
}

std::u16string entity_report(const Entity& e) {
  std::vector<std::u16string> lines;
  const auto add = [&](const char16_t* key, const std::u16string& value) {
    const std::u16string k = key;
    lines.push_back(k + std::u16string(k.size() < kKeyWidth ? kKeyWidth - k.size() : 0, u' ') +
                    value);
  };

  const std::u16string data = to_string(e.data_id());
  add(u"data", usable(data) ? data : std::u16string(u"-"));
  std::u16string name = to_string(e.name());
  name = usable(name) ? u"\"" + name + u"\"" : std::u16string(u"-");
  add(u"name", name + u"  visible=" + num(e.name_visible));
  const std::u16string group = to_string(e.group());
  add(u"team", e.team() + u"  group=" + (usable(group) ? group : std::u16string(u"-")));
  add(u"position", vec3(e.position) + u"  prev=" + vec3(e.prev_position));
  add(u"velocity", vec3(e.velocity) + u"  prev=" + vec3(e.prev_velocity));
  std::u16string state = e.state_ptr() != nullptr ? to_string(e.state_ptr()->state())
                                                 : std::u16string(u"-");
  std::u16string frame_line = e.frame_id() + u"  state=" + state + u"  wait=" + num(e.wait);
  if (e.frame != nullptr && e.frame->valid()) {
    const std::u16string frame_state = to_string(e.frame->state());
    if (frame_state != state) frame_line += u"  frame_state=" + frame_state;
  }
  add(u"frame", frame_line);
  add(u"hp/mp", num(e.hp()) + u"/" + num(e.hp_max()) + u"  " + num(e.mp()) + u"/" +
                    num(e.mp_max()));
  add(u"facing", num(e.facing) + u"  on_ground=" + (e.is_on_ground ? u"1" : u"0"));
  add(u"flags", u"invisible=" + num(e.invisible()) + u"  invulnerable=" + num(e.invulnerable()) +
                    u"  shaking=" + num(e.shaking) + u"  motionless=" + num(e.motionless) +
                    u"  arest=" + num(e.arest()) + u"  reserve=" + num(e.reserve()));
  add(u"other", u"ctrl=" + ctrl_name(e) + u"  puppet=" + (e.puppet ? u"1" : u"0") +
                    u"  ghosted=" + num(e.ghosted()) + u"  buffs=" +
                    num(static_cast<double>(e.buffs.size())) + u"  lifetime=" +
                    num(e.lifetime()));
  add(u"link", u"holding=" + entity_ref(e.holding) + u"  bearer=" + entity_ref(e.bearer) +
                   u"  catching=" + entity_ref(e.catching) + u"  catcher=" + entity_ref(e.catcher));

  std::u16string out = u"entity #" + e.id;
  for (const std::u16string& line : lines) out += u"\n  " + line;
  return out;
}

void apply_option(Entity& e, LFW& lfw, const std::u16string& key, const std::u16string& text) {
  if (key == u"x" || key == u"y" || key == u"z") {
    const double v = string_to_number(text);
    if (std::isnan(v)) {
      lfw.warn(u"ENTITY: --" + key + u" needs a number, got \"" + text + u"\"");
      return;
    }
    // 只给一个轴时其余保持原值；`set_position` 内部会按场地边界限制并刷 `ground_y`
    e.set_position(key == u"x" ? Value(v) : Value(e.position.x),
                   key == u"y" ? Value(v) : Value(e.position.y),
                   key == u"z" ? Value(v) : Value(e.position.z));
    return;
  }
  if (key == u"velocity") {
    const std::vector<double> v = parse_nums(text);
    if (v.size() != 3 || std::isnan(v[0]) || std::isnan(v[1]) || std::isnan(v[2])) {
      lfw.warn(u"ENTITY: --velocity needs x,y,z, got \"" + text + u"\"");
      return;
    }
    e.velocity.set(v[0], v[1], v[2]);
    e.prev_velocity.set(v[0], v[1], v[2]);
    return;
  }
  if (key == u"frame") {
    const bool special = text.empty() || text == frame_id::kAuto || text == frame_id::kSelf ||
                         text == frame_id::kGone;
    if (!special && e.data_frame(text) == nullptr) {
      lfw.warn(u"ENTITY: data " + to_string(e.data_id()) + u" has no frame \"" + text + u"\"");
      return;
    }
    const EnterFrameResult r = e.enter_frame_by_id(Value(text), false);
    if (r != EnterFrameResult::Entered) {
      lfw.warn(u"ENTITY: frame \"" + text + u"\" -> " + enter_frame_result_name(r));
    }
    return;
  }
  if (key == u"state") {
    const Value code = value_of(text);
    if (!state::entity_states().has(code)) {
      lfw.warn(u"ENTITY: no such state code \"" + text + u"\"");
      return;
    }
    e.set_state(code);
    return;
  }
  if (key == u"name") {
    e.set_name(text.empty() ? Value() : Value(text));
    return;
  }
  if (key == u"name_visible") {
    e.name_visible = to_number(value_of(text));
    return;
  }
  if (key == u"holding" || key == u"bearer") {
    // 空 = 断开；其余按实体 id 连起来（`#` 可省）。数字也是 id，别走 `set_prop` 的「数字 = 断开」
    const bool release = text.empty() || text == u"null" || text == u"none";
    e.set_prop(key, release ? Value() : Value(text));
    return;
  }
  if (e.set_prop(key, value_of(text))) return;
  lfw.warn(u"ENTITY: unknown option --" + key + u"（`help entity` 看支持哪些）");
}

}  // namespace

const char16_t* cmd_entity_help() {
  return uR"(Usage: ENTITY <id> [--<prop>=<value> ...]

Show one entity; every option given is applied first, then the result is printed
显示指定实体的状态；带了选项就先把改动应用掉，再打印改后的状态（id 用 `ENTITIES` 里那个，`#` 可省）

Geometry / 位置与速度:
  --x=<n> --y=<n> --z=<n>      position（只给其中几个时其余不变）
  --velocity=<x,y,z>           velocity / 速度
Frame & name / 帧与名字:
  --frame=<frame_id>           enter a frame id / 切到数据里的某个帧 id
  --state=<state_code>         set state / 设置状态码
  --name=<text>                display name / 显示名（空 = 回到数据里的名字）
  --name_visible=<0|1>         draw the name or not / 是否显示名字
Props / 属性（同 `A_SET_PROP` 那套）:
  hp hp_r hp_max mp mp_max invisible invulnerable toughness fallinjury
  throwinjury motionless shaking facing arest dropping team reserve
  stat_bar
  holding / bearer 也可以写实体 id（`--holding=#102`）连起来；写数字或空 = 断开)";
}

void cmd_entity(CMDS& ctx) {
  World& world = ctx.world();
  LFW& lfw = world.lfw();
  const std::optional<std::u16string> raw = ctx.str(1);
  if (!raw.has_value() || raw->empty()) {
    lfw.warn(u"ENTITY failed, must \"ENTITY <id> [--prop=value ...]\", got: " + ctx.cmd());
    return;
  }
  Entity* const e = find_entity_any(world, *raw);
  if (e == nullptr) {
    lfw.warn(u"ENTITY failed, no such entity: " + *raw + u"（`ENTITIES --all` 看有哪些）");
    return;
  }
  for (const std::u16string& word : ctx.words()) {
    if (!starts_with_dash(word)) continue;
    const size_t eq = word.find(u'=');
    if (eq == std::u16string::npos || eq <= 2) {
      lfw.warn(u"ENTITY: option needs a value: " + word + u"（例：--hp=100）");
      continue;
    }
    apply_option(*e, lfw, word.substr(2, eq - 2), word.substr(eq + 1));
  }
  lfw.log(entity_report(*e));
}

const char16_t* cmd_despawn_help() {
  return uR"(Usage: DESPAWN <entity_id> ...

Remove entities directly by id / 按 id 直接移除实体)";
}

void cmd_despawn(CMDS& ctx) {
  World& world = ctx.world();
  const std::vector<std::u16string>& words = ctx.words();
  for (size_t i = 1; i < words.size(); ++i) {
    const std::u16string& id = words[i];
    if (id.empty() || starts_with_dash(id)) continue;
    Entity* const e = world.find_entity(id);
    if (e == nullptr) continue;
    world.del_entity(*e);
  }
}

const char16_t* cmd_del_puppet_help() {
  return uR"(Usage: DEL_PUPPET <player_id>

Remove a player's puppet / 移除指定玩家的傀儡)";
}

void cmd_del_puppet(CMDS& ctx) {
  World& world = ctx.world();
  const std::optional<std::u16string> player_id = ctx.str(1);
  if (!player_id.has_value()) {
    world.lfw().warn(u"DEL_PUPPET failed, must \"DEL_PUPPET ${playerId}\", got: " + ctx.cmd());
    return;
  }
  Entity* entity = nullptr;
  for (const std::pair<std::u16string, Entity*>& kv : world.puppets) {
    if (kv.first == *player_id) entity = kv.second;
  }
  if (entity != nullptr) {
    world.del_entity(*entity);
  } else {
    world.lfw().warn(u"DEL_PUPPET failed, puppet not found.");
  }
}

}
}
