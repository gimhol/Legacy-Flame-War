#include "lfw/cmds/cmd_scene.h"
#include "lfw/lfw.h"

#include <optional>
#include <string>

#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/stage/stage.h"
#include "lfw/world.h"
#include "lfw/world_dataset.h"

namespace lfw {
namespace cmds {

const char16_t* cmd_bgm_help() {
  return uR"(Usage: BGM <id|?>

Play a BGM, '?' plays a random next one / 播放 BGM，'?' 随机播放下一首)";
}

void cmd_bgm(CMDS& ctx) {
  const std::optional<std::u16string> id = ctx.str(1);
  ctx.world().lfw().sounds_play_bgm(Value(id.has_value() ? *id : std::u16string(u"?")));
}

const char16_t* cmd_change_bg_help() {
  return uR"(Usage: CHANGE_BG <bg_id>

Change the background / 切换背景)";
}

void cmd_change_bg(CMDS& ctx) {
  const std::optional<std::u16string> id = ctx.str(1);
  ctx.world().change_bg(id.has_value() ? Value(*id) : Value());
}

const char16_t* cmd_change_stage_help() {
  return uR"(Usage: CHANGE_STAGE <stage_id>

Change the stage / 切换关卡)";
}

void cmd_change_stage(CMDS& ctx) {
  const std::optional<std::u16string> id = ctx.str(1);
  ctx.world().change_stage(id.has_value() ? Value(*id) : Value());
}

const char16_t* cmd_set_difficulty_help() {
  return uR"(Usage: SET_DIFFICULTY <1|2|3|4>

Set the game difficulty / 设置游戏难度)";
}

void cmd_set_difficulty(CMDS& ctx) {
  const std::optional<double> d = ctx.num(1);
  if (!d.has_value() || !defines::is_difficulty(*d)) {
    ctx.world().lfw().warn(u"SET_DIFFICULTY failed, must \"SET_DIFFICULTY ${1|2|3|4}\", got: " +
                            ctx.cmd());
    return;
  }
  ctx.world().dataset.set(u"difficulty", Value(*d));
}

const char16_t* cmd_set_lang_help() {
  return uR"(Usage: SET_LANG <lang>

Switch UI language at runtime ('zh-cn' / 'en-us' ...), the current page re-translates at once
运行时切换 UI 语言（'zh-cn' / 'en-us' …），当前页面立刻按新语言重解析文本)";
}

void cmd_set_lang(CMDS& ctx) {
  const std::optional<std::u16string> code = ctx.str(1);
  if (!code.has_value() || code->empty()) {
    ctx.world().lfw().warn(u"SET_LANG failed, must \"SET_LANG <zh-cn|en-us|...>\", got: " + ctx.cmd());
    return;
  }
  ctx.world().lfw().set_lang(Value(*code));
  ctx.world().lfw().log(u"lang -> " + ctx.world().lfw().lang());
}

}
}
