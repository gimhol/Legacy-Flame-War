#include "host/godot/lfw_godot_node.h"
#include "host/godot/lfw_godot_ui_run.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include "host/common/key_code.h"
#include "host/common/lfw_boot.h"
#include "host/common/ui_input_handle.h"
#include "lfw/base/clock.h"
#include "lfw/base/perf.h"
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/cmd.h"
#include "lfw/defines/sync_render_enum.h"
#include "lfw/entity/gen_field.h"
#include "lfw/ui/action/actor.h"
#include "lfw/ui/ui_action_enum.h"
#include "lfw/ui/component/scroll_view.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

#include "host/godot/lfw_godot_node_internal.h"

namespace lfw::host {

namespace {

using godot::String;
using godot::UtilityFunctions;
using godot::Color;
using godot::Rect2;
using godot::Vector2;

using godot_node::from_utf8;
using godot_node::to_utf8;
using godot_node::print_line;
using godot_node::print_error;
using godot_node::wall_now_ms;
using godot_node::is_volume_control;
using godot_node::lower_ascii;
using godot_node::read_file_bytes;
using godot_node::find_repo_root;
using godot_node::exe_dir;
using godot_node::packaged_resources_dir;
using godot_node::user_mods_dir;
using godot_node::user_remote_cache_dir;
using godot_node::pending_custom_game_file;
using godot_node::cwd_string;
using godot_node::titlebar_name;
using godot_node::to_window_edge;
using godot_node::window_state_text;



/** 没给 `--root` / `--zip` 时用打包目录补齐；返回是否补到了数据包。 */
bool apply_packaged_defaults(std::vector<std::filesystem::path>& roots,
                             std::vector<std::filesystem::path>& zips) {
  const std::filesystem::path res = packaged_resources_dir();
  if (res.empty()) return false;
  std::error_code ec;
  if (roots.empty() && std::filesystem::is_directory(res / "root", ec))
    roots.push_back(res / "root");
  if (zips.empty()) {
    for (const char* name : {"prel", "data"}) {
      const std::filesystem::path archive = res / (std::string(name) + ".zip");
      const std::filesystem::path dir = res / name;
      if (std::filesystem::is_regular_file(archive, ec)) {
        zips.push_back(archive);
      } else if (std::filesystem::is_directory(dir, ec)) {
        zips.push_back(dir);
      }
    }
  }
  return !zips.empty();
}

/** `mods.json5` / `mods.json`：`order` 决定加载顺序，`disabled` 里的不加载 */
struct ModsConfig {
  std::vector<std::string> order;
  std::vector<std::string> disabled;
};

std::vector<std::string> string_list(const Value* const v) {
  std::vector<std::string> out;
  if (v == nullptr) return out;
  if (const Array* const arr = as_array(*v); arr != nullptr) {
    for (size_t i = 0; i < arr->size(); ++i) {
      if (const std::u16string* const s = std::get_if<std::u16string>(&arr->at(i)))
        out.push_back(to_utf8(*s));
    }
    return out;
  }
  if (const std::u16string* const one = std::get_if<std::u16string>(v)) out.push_back(to_utf8(*one));
  return out;
}

ModsConfig read_mods_config(const std::filesystem::path& dir) {
  ModsConfig ret;
  for (const char* name : {"mods.json5", "mods.json"}) {
    std::error_code ec;
    const std::filesystem::path path = dir / name;
    if (!std::filesystem::is_regular_file(path, ec)) continue;
    std::string raw;
    if (!read_file_bytes(path, raw)) break;
    const lfw::Json5Result parsed = lfw::json5_parse(from_utf8(raw));
    if (!parsed.ok) {
      print_error("[godot] mods: bad " + path.generic_string() + " (" + to_utf8(parsed.error) + ")");
      break;
    }
    if (const Object* const obj = as_object(parsed.value); obj != nullptr) {
      ret.order = string_list(obj->get(u"order"));
      ret.disabled = string_list(obj->get(u"disabled"));
    }
    break;
  }
  return ret;
}

/** 一个模组目录里的 `.zip`：`order` 里列出的按该顺序在前，其余按文件名在后 */
std::vector<std::filesystem::path> scan_mods_dir(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> out;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return out;
  std::vector<std::string> names;
  for (std::filesystem::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    std::error_code file_ec;
    if (!it->is_regular_file(file_ec)) continue;
    std::string name = it->path().filename().string();
    if (name.size() <= 4 || lower_ascii(name.substr(name.size() - 4)) != ".zip") continue;
    names.push_back(std::move(name));
  }
  if (names.empty()) return out;

  const ModsConfig conf = read_mods_config(dir);
  std::map<std::string, size_t> rank;  // 先列出的先加载
  for (const std::string& v : conf.order) rank.emplace(lower_ascii(v), rank.size());
  std::vector<std::string> disabled;
  for (const std::string& v : conf.disabled) disabled.push_back(lower_ascii(v));

  std::vector<std::string> keep;
  for (const std::string& name : names) {
    const std::string key = lower_ascii(name);
    if (std::find(disabled.begin(), disabled.end(), key) != disabled.end()) continue;
    keep.push_back(name);
  }
  const auto rank_of = [&rank](const std::string& name) {
    const auto found = rank.find(lower_ascii(name));
    return found == rank.end() ? std::numeric_limits<size_t>::max() : found->second;
  };
  std::stable_sort(keep.begin(), keep.end(), [&](const std::string& a, const std::string& b) {
    const size_t ra = rank_of(a), rb = rank_of(b);
    if (ra != rb) return ra < rb;
    return lower_ascii(a) < lower_ascii(b);
  });
  for (const std::string& name : keep) out.push_back(dir / name);
  return out;
}

/**
 * 模组目录（越后面优先级越高）：`<exe>/mods` → `<cwd>/mods` → 用户数据目录 `mods/` → `--mods`。
 * 扫到的 `*.zip` 单独存进 `mods`（它们可能是「自定义游戏包」，要在 `setup()` 里分类）。
 */
void append_mods(const std::vector<std::filesystem::path>& extra_dirs, bool no_mods,
                 std::vector<std::filesystem::path>& mods) {
  if (no_mods) return;
  std::vector<std::filesystem::path> dirs;
  const std::filesystem::path exe = exe_dir();
  if (!exe.empty()) dirs.push_back(exe / "mods");
  std::error_code ec;
  const std::filesystem::path cwd = std::filesystem::current_path(ec);
  // 开发时（直接跑 Godot 二进制）`<exe>/mods` 是 Godot 桌面，顺手也认当前目录下的 mods/
  if (!ec && !cwd.empty() && cwd != exe) dirs.push_back(cwd / "mods");
  if (const std::filesystem::path user = user_mods_dir(); !user.empty()) dirs.push_back(user);
  for (const std::filesystem::path& dir : extra_dirs) dirs.push_back(dir);
  if (dirs.empty()) return;

  size_t loaded = 0;
  std::string names;
  std::vector<std::filesystem::path> seen;
  bool any_dir = false;
  for (const std::filesystem::path& dir : dirs) {
    std::error_code dir_ec;
    if (!std::filesystem::is_directory(dir, dir_ec)) continue;
    any_dir = true;
    for (const std::filesystem::path& zip : scan_mods_dir(dir)) {
      std::error_code canon_ec;
      const std::filesystem::path canon = std::filesystem::weakly_canonical(zip, canon_ec);
      const std::filesystem::path key = canon_ec ? zip : canon;
      if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
      seen.push_back(key);
      mods.push_back(zip);
      if (!names.empty()) names += ", ";
      names += zip.filename().generic_string();
      ++loaded;
    }
  }
  if (!any_dir) return;
  if (loaded == 0) {
    print_line("[godot] mods: no .zip in " + dirs.front().generic_string());
    return;
  }
  print_line("[godot] mods: loaded " + std::to_string(loaded) + " (" + names + ")");
}

bool parse_click_spec(const std::string& text, int default_frame, double& x, double& y, int& frame) {  const size_t at = text.find('@');
  const std::string pos = at == std::string::npos ? text : text.substr(0, at);
  const size_t comma = pos.find(',');
  if (comma == std::string::npos) return false;
  x = std::strtod(pos.substr(0, comma).c_str(), nullptr);
  y = std::strtod(pos.substr(comma + 1).c_str(), nullptr);
  if (!std::isfinite(x) || !std::isfinite(y)) return false;
  frame = default_frame;
  if (at != std::string::npos) frame = std::atoi(text.substr(at + 1).c_str());
  return frame >= 0;
}

}


const char* LfwGodot::usage() {
  return "usage: <godot> --path native/host/godot/project -- --root <dir> --zip <dir>\n"
         "               [--zip <dir> ...] [--fighter <id>] [--stage <id>] [--players N]\n"
         "               [--frames N] [--every N] [--time ms] [--delta seconds] [--vs]\n"
         "               [--play] [--click <x>,<y>[@<frame>]] [--click-window <x>,<y>[@<frame>]]\n"
         "               [--hover-window <x>,<y>[@<frame>]] [--click-frame N]\n"
         "               [--titlebar native|none|custom|frame] [--window-scale N]\n"
         "               [--window-size WxH] [--icons <dir>] [--mods <dir>] [--no-mods]\n"
         "               [--key <name>[@<frame>]] [--key-down <name>[@<frame>]]\n"
         "               [--key-up <name>[@<frame>]] [--key-frame N] [--key-tap N]\n"
         "               [--lang <code>] [--version-name <text>]\n"
         "               [--screenshot <file.png>] [--shot-frame N] [--ui-dump] [--ui-page <id>]\n"
         "               [--no-ui] [--version-check <url>] [--no-remote-cache]\n"
         "  --root     import 根目录（含 builtin_data/…），可重复；缺省 `.`\n"
         "  --zip      数据包：解包目录、`.zip` 文件，或远端 `http(s)://…/xxx.zip.json`（info json，\n"
         "             走 HTTP + `user://cache/data` 落盘缓存；带 Range 断点续传、md5 校验），可重复\n"
         "  --version-check  取远端 `latest.json`（`{ \"version\": \"v0.1.65\" }`）比版本，结果只打日志；\n"
         "             TTL 6 小时（缓存在 `user://cache/data/version.json`）\n"
         "  --no-remote-cache  不启用落盘缓存（每次强制联网；诊断用）\n"
         "  --mods     额外模组目录（放 *.zip 的目录），可重复；比默认目录后加载 = 优先级更高\n"
         "  --drop     诊断：启动后按「拖拽」路径把一个 .zip 当模组热加载（和窗口里拖入等价），可重复\n"
         "  --custom-game  诊断：启动后走「自定义游戏」路径（暂存 + 重启）加载一个 full.zip，可重复\n"
         "  --no-mods  不自动加载模组（默认目录与 `--mods` 都跳过）\n"
         "  --console  启动时就把控制台（Ctrl+`）打开（诊断 / 截图用）\n"
         "  --console-echo  把控制台收到的日志行同时打到 stdout（headless 看命令输出用）\n"
         "  --console-exec <行>[@<帧>]  按控制台输入执行一条命令（缺省启动时；`@帧` 排到那一帧），\n"
         "             可重复。命令输出走 `log` 通道 ⇒ 配 `--console-echo` 才在 stdout 里\n"
         "  --console-text <文本>[@<帧>]  只把文本填进控制台输入框（不执行），打一行当前命令建议，\n"
         "             可重复（截图 / 验收输入建议用）\n"
         "             默认模组目录（越后面优先）：`<exe>/mods` → `<cwd>/mods` →\n"
         "             用户数据目录 `mods/`（Godot 的 user://mods）。内含 `index.json` 的\n"
         "             zip 是「自定义游戏包」，会整包替换本体数据（只认第一个）。\n"
         "  --lang     UI 语言码（`zh-cn` / `en-us`…，与 Web 版的 `navigator.language.toLowerCase()`\n"
         "             同一口径；走 `LFW::set_lang`，**在 `load` 之前**设）。缺省 = 系统区域\n"
         "             （`OS::get_locale()`，`_` 换 `-`）。\n"
         "  --version-name  左下角那行版本号（`init.ui.json` 的 `version_name` 节点读 `VERSION_NAME`）。\n"
         "             缺省 = 读 `<root>/version.json` 拼成 Web 版同一串 `v<版本>-<commit7>[-dirty] <时间>`；\n"
         "             读不到就是 `v0.0.0`。\n"
         "  --fighter  指定出场的角色 id，可重复（缺省用数据包里的前 `--players` 个）\n"
         "  --play     开放模式：不 `boot_scene`（由数据入口页 `init` 驱动）、不限帧数（关窗口才\n"
         "             结束）、缺省不打帧摘要、节拍用 Godot 的真 `delta`。= `--frames 0` + 不造场景。\n"
         "  --frames   推这么多逻辑帧就收工并打摘要（验收模式）；`--frames 0` = 不限帧数\n"
         "  --delta    只覆盖 `_process` 的节拍（秒），不改固定步长（默认用 Godot 给的 delta）\n"
         "  --click    在第 `--click-frame` 帧（或 `@frame`）注入一次完整点击（按下 + 抬起）；\n"
         "             坐标是 UI 逻辑像素（左上原点、794×450 那套，与截图 1:1）。可重复。\n"
         "  --click-window  同上，但坐标是**窗口像素**：走 `push_input(ev, local=false)` ⇒ 引擎按\n"
         "             窗口缩放反算回逻辑像素（与 OS 点击同一条路）—— 证「换算对不对」用这条。\n"
         "  --hover-window  只推一条 `InputEventMouseMotion`（窗口像素）—— 悬停态（顶栏的拖拽层高亮、\n"
         "             控件缩 0.8、音量弹层）只有它能推；`--click*` 只推按钮事件。\n"
         "  --key          在第 `--key-frame` 帧（或 `@frame`）按一下（按下 + `--key-tap` 帧后抬起），\n"
         "             走 `HeadlessHost::key_down/key_up`（与真键盘的 `_input` 同一条链）；\n"
         "             键名取小写 `KeyboardEvent.key`（`space` / `esc` / `up` / `j`…）。可重复。\n"
         "  --key-down/--key-up  只按下 / 只抬起（要「按住」就用这一对）。\n"
         "  --key-frame    没写 `@frame` 的按键落在哪一帧（缺省 60）\n"
         "  --key-tap      「按一下」的按下 → 抬起间隔（帧，缺省 2）\n"
         "  --titlebar 窗口那一套：native（系统边框 + 系统标题栏）/ none（无边框、不画栏）/ custom\n"
         "             （无边框 + 自绘栏：11 个 `btn_*.png` 控件 + 拖拽层 +\n"
         "             音量弹层）/ frame（**原生边框但不要系统标题栏**：自己换窗口过程、`WM_NCCALCSIZE`\n"
         "             只摘 caption、边框留着）+ 自绘栏）。\n"
         "             缺省：`--play` 时 frame，其余 native。\n"
         "  --icons    顶栏那 11 张 `btn_*.png` 的目录（缺省 = 从仓库根解析出来的 `src/assets`；\n"
         "             它们**不在数据包里**，导出/打包时得自己带上）。\n"
         "  --display-scale  覆盖顶栏那一层用的显示缩放（CSS 像素 → 窗口物理像素；缺省 = 屏幕自己的\n"
         "             `DisplayServer::screen_get_scale()`）。诊断用：屏幕缩放恰为 1 时靠它把 DPI 那一档\n"
         "             跑起来（1.5 ⇒ 条高 32×1.5 = 48 物理像素、图 19.2×1.5 = 28.8）。\n"
         "  --window-scale  窗口 = 逻辑屏幕（794×450）× N；逻辑屏幕不变（`project.godot` 的 viewport_*）。\n"
         "  --window-size  直接给窗口像素尺寸 `WxH`（可不等比，压过 `--window-scale`）；窗口可自由拉伸，\n"
         "             下限是逻辑屏幕 794×450；画面由宿主按 contain + 居中摆进窗口（补黑边），\n"
         "             自绘栏铺满整窗宽、贴窗口右上角。\n"
         "             缺省：`--play` 时 1.5（= 1191×675），其余 1.0。\n"
         "  --screenshot  画到第 `--shot-frame` 帧后存一张 PNG（整窗：黑边 + 条 + 画面；\n"
         "                **别加 --headless**：headless 没有渲染，存出来是空白）；给绝对路径\n"
         "  --shot-frame  截图的帧号（缺省 120）\n"
         "  --ui-dump     最后一帧逐 UI 节点打一行事实（诊断，不进摘要）\n"
         "  --integer-scale  内容按 floor(缩放比) 的**整数倍**摆放、余量补黑边。像素画在\n"
         "             非整数放大（1080p 全屏是 2.4x）下每个源像素会落在 2/3 px 之间来回变；\n"
         "             开了这个就每个像素都等宽。缺省关（画面铺满窗口）。\n"
         "  --clip-probe  在逻辑屏幕边界内外各 8 像素贴一圈洋红（画在游戏之上）：证明游戏那一层\n"
         "                真的被裁在内容矩形里（看得见边界**里面**那半圈、黑边干净 = 裁剪生效）。\n"
         "  --ui-page     装载后把这一页设到第 0 层（id 取自烹饪好的 UI 表；缺省用首页 `init`）\n"
         "  --stats       启动时就把左上角性能面板打开（等价于设置页「性能数据」开成启用）\n"
         "  --steam       0 / 1 / auto（缺省 auto）：接 Steamworks。只有编译期带了 SDK\n"
         "                （`host/godot/third/steam` + `LFW_GODOT_STEAM=ON`）才可能真开；\n"
         "                `steam_api64.dll` 不在 / Steam 没登录 / 没有 appid ⇒ 记一行原因后关掉\n"
         "  --steam-overlay  初始化成功后立刻弹一次 Steam Overlay（诊断；游戏里 Shift+Tab 也能弹）\n"
         "  --drag-window <x0>,<y0>,<x1>,<y1>[@<frame>]  窗口像素左键拖拽（控制台选字等验收用）\n"
         "  --sync-render 强制渲染频率模式（诊断）：0 无限 / 1 减半 / 2 同步 / 3 60FPS / 4 120FPS，\n"
         "                缺省用数据集里的 `sync_render`（设置页「帧率」那一行）\n"
         "  --text-input  脚本化打字：`<text>[@<frame>]`。第 `frame` 帧让文本输入覆盖层（Web 的\n"
         "                DOM `<input>` 在 Godot 侧的等价物：一个真的 `LineEdit`）拿焦点并整串写入，\n"
         "                `+2` 帧放掉焦点（⇒ 组件 `on_blur` 存盘），`+4` 帧打一行事实。缺省帧 60。\n"
         "  打包版（免安装目录）：不带任何参数时等价于 `-- --play`，`--root/--zip/--icons`\n"
         "             自动取 `<exe 所在目录>/resources/{root,prel,data,icons}`，\n"
         "             `<exe 所在目录>/mods/*.zip` 自动作为模组加载。\n";
}

std::vector<std::string> LfwGodot::user_args() {
  std::vector<std::string> out;
  const godot::PackedStringArray args = godot::OS::get_singleton()->get_cmdline_user_args();
  for (int64_t i = 0; i < args.size(); ++i) {
    const String arg = args[i];
    out.emplace_back(arg.utf8().get_data());
  }
  return out;
}

bool LfwGodot::parse_args(const std::vector<std::string>& argv, Args& out) const {
  const auto need = [&](size_t& i, std::string& value) -> bool {
    if (i + 1 >= argv.size()) return false;
    value = argv[++i];
    return true;
  };
  // `--console-exec` / `--console-text` 的 `<文本>[@<帧>]`：结尾是纯数字才算帧号，
  // 否则整串都是文本（`DATASET@150` 排到第 150 帧）
  const auto console_spec = [](const std::string& text) {
    ConsoleSpec spec;
    spec.line = text;
    const size_t at = text.rfind('@');
    if (at == std::string::npos || at + 1 >= text.size()) return spec;
    const std::string tail = text.substr(at + 1);
    const bool digits =
        std::all_of(tail.begin(), tail.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if (!digits) return spec;
    spec.line = text.substr(0, at);
    spec.frame = std::atoi(tail.c_str());
    return spec;
  };
  for (size_t i = 0; i < argv.size(); ++i) {
    const std::string& a = argv[i];
    std::string v;
    if (a == "--root") {
      if (!need(i, v)) return false;
      out.roots.emplace_back(v);
    } else if (a == "--zip") {
      if (!need(i, v)) return false;
      const std::u16string url = from_utf8(v);
      if (url.rfind(u"http://", 0) == 0 || url.rfind(u"https://", 0) == 0) {
        out.remote_zips.push_back(url);
      } else {
        out.zips.emplace_back(v);
      }
    } else if (a == "--version-check") {
      if (!need(i, v)) return false;
      out.version_check_url = from_utf8(v);
    } else if (a == "--no-remote-cache") {
      out.no_remote_cache = true;
    } else if (a == "--mods") {
      if (!need(i, v)) return false;
      out.mod_dirs.emplace_back(v);
    } else if (a == "--drop") {
      if (!need(i, v)) return false;
      out.drop_zips.emplace_back(v);
    } else if (a == "--custom-game") {
      if (!need(i, v)) return false;
      out.custom_games.emplace_back(v);
    } else if (a == "--no-mods") {
      out.no_mods = true;
    } else if (a == "--console") {
      out.console = true;
    } else if (a == "--console-echo") {
      out.console_echo = true;
    } else if (a == "--console-exec") {
      if (!need(i, v)) return false;
      out.console_exec.push_back(console_spec(v));
    } else if (a == "--console-text") {
      if (!need(i, v)) return false;
      out.console_text.push_back(console_spec(v));
    } else if (a == "--fighter") {
      if (!need(i, v)) return false;
      out.fighters.push_back(from_utf8(v));
    } else if (a == "--stage") {
      if (!need(i, v)) return false;
      out.stage = from_utf8(v);
    } else if (a == "--frames") {
      if (!need(i, v)) return false;
      out.frames = std::atoi(v.c_str());
    } else if (a == "--players") {
      if (!need(i, v)) return false;
      out.players = std::atoi(v.c_str());
    } else if (a == "--every") {
      if (!need(i, v)) return false;
      out.every = std::atoi(v.c_str());
    } else if (a == "--time") {
      if (!need(i, v)) return false;
      out.time = std::atof(v.c_str());
    } else if (a == "--delta") {
      if (!need(i, v)) return false;
      out.delta = std::atof(v.c_str());
    } else if (a == "--screenshot") {
      if (!need(i, v)) return false;
      out.screenshot = std::filesystem::path(v);
    } else if (a == "--shot-frame") {
      if (!need(i, v)) return false;
      out.shot_frame = std::atoi(v.c_str());
    } else if (a == "--vs") {
      out.vs = true;
    } else if (a == "--lang") {
      if (!need(i, v)) return false;
      out.lang = from_utf8(v);
    } else if (a == "--version-name") {
      if (!need(i, v)) return false;
      out.version_name = v;
    } else if (a == "--play") {
      out.play = true;
      out.frames = 0;
    } else if (a == "--click") {
      if (!need(i, v)) return false;
      out.click_specs.push_back(v);
    } else if (a == "--click-window") {
      if (!need(i, v)) return false;
      out.window_click_specs.push_back(v);
    } else if (a == "--hover-window") {
      if (!need(i, v)) return false;
      out.hover_specs.push_back(v);
    } else if (a == "--drag-window") {
      if (!need(i, v)) return false;
      out.drag_specs.push_back(v);
    } else if (a == "--titlebar") {
      if (!need(i, v)) return false;
      if (v == "native") {
        out.titlebar = TitlebarMode::native;
      } else if (v == "none") {
        out.titlebar = TitlebarMode::none;
      } else if (v == "custom") {
        out.titlebar = TitlebarMode::custom;
      } else if (v == "frame") {
        out.titlebar = TitlebarMode::frame;
      } else {
        print_error("[godot] bad --titlebar value: " + v + " (want native|none|custom|frame)");
        return false;
      }
      out.titlebar_given = true;
    } else if (a == "--window-scale") {
      if (!need(i, v)) return false;
      out.window_scale = std::atof(v.c_str());
      if (!(out.window_scale > 0.0) || out.window_scale > 8.0) {
        print_error("[godot] bad --window-scale value: " + v + " (want 0 < N <= 8)");
        return false;
      }
      out.window_scale_given = true;
    } else if (a == "--window-size") {
      if (!need(i, v)) return false;
      int w = 0;
      int h = 0;
      if (std::sscanf(v.c_str(), "%dx%d", &w, &h) != 2 || w < 1 || h < 1 || w > 16384 || h > 16384) {
        print_error("[godot] bad --window-size value: " + v + " (want WxH, e.g. 1191x675)");
        return false;
      }
      out.window_w = w;
      out.window_h = h;
    } else if (a == "--icons") {
      if (!need(i, v)) return false;
      out.icons = std::filesystem::path(v);
      out.icons_given = true;
    } else if (a == "--click-frame") {
      if (!need(i, v)) return false;
      out.click_frame = std::atoi(v.c_str());
      if (out.click_frame < 0) return false;
    } else if (a == "--key" || a == "--key-down" || a == "--key-up") {
      if (!need(i, v)) return false;
      out.key_specs.push_back(Args::KeySpec{
          a == "--key" ? "tap" : (a == "--key-down" ? "down" : "up"), v});
    } else if (a == "--key-frame") {
      if (!need(i, v)) return false;
      out.key_frame = std::atoi(v.c_str());
      if (out.key_frame < 0) return false;
    } else if (a == "--key-tap") {
      if (!need(i, v)) return false;
      out.key_tap = std::atoi(v.c_str());
      if (out.key_tap < 1) return false;
    } else if (a == "--ui-dump") {
      out.ui_dump = true;
    } else if (a == "--integer-scale") {
      out.integer_scale = true;
    } else if (a == "--clip-probe") {
      out.clip_probe = true;
    } else if (a == "--stats") {
      out.stats = true;
    } else if (a == "--steam-overlay") {
      out.steam_overlay = true;
    } else if (a == "--steam") {
      if (!need(i, v)) return false;
      if (v == "0" || v == "off") {
        out.steam = SteamMode::kOff;
      } else if (v == "1" || v == "on") {
        out.steam = SteamMode::kOn;
      } else if (v == "auto") {
        out.steam = SteamMode::kAuto;
      } else {
        print_error("[godot] bad --steam value: " + v + " (want 0|1|auto)");
        return false;
      }
    } else if (a == "--display-scale") {
      if (!need(i, v)) return false;
      out.display_scale = std::strtod(v.c_str(), nullptr);
      if (!(out.display_scale > 0.0)) return false;
    } else if (a == "--ui-scroll") {
      if (!need(i, v)) return false;
      out.ui_scroll = v;
    } else if (a == "--sync-render") {
      if (!need(i, v)) return false;
      out.sync_render = std::atoi(v.c_str());
      if (out.sync_render < 0 || out.sync_render > 4) return false;
    } else if (a == "--ui-page") {
      if (!need(i, v)) return false;
      out.ui_page = from_utf8(v);
    } else if (a == "--no-ui") {
      out.no_ui = true;
    } else if (a == "--text-input") {
      if (!need(i, v)) return false;
      std::string text = v;
      const std::size_t at = text.rfind('@');
      if (at != std::string::npos && at + 1 < text.size() &&
          text.find_first_not_of("0123456789", at + 1) == std::string::npos) {
        out.text_input_frame = std::atoi(text.c_str() + at + 1);
        text.resize(at);
      }
      out.text_input = from_utf8(text);
      out.text_input_given = true;
    } else {
      print_error("[godot] unknown argument: " + a);
      return false;
    }
  }
  // 打包版：`<exe>/resources` 里的 root/prel/data 自动补上（命令行显式给的优先）
  apply_packaged_defaults(out.roots, out.zips);
  // 模组：默认两个目录 + `--mods`，单独一列（其中一个可能是「自定义游戏包」）
  append_mods(out.mod_dirs, out.no_mods, out.mod_zips);
  if (out.roots.empty()) out.roots.emplace_back(".");
  if (!out.titlebar_given) out.titlebar = out.play ? TitlebarMode::frame : TitlebarMode::native;
  if (!out.window_scale_given) out.window_scale = out.play ? 1.5 : 1.0;
  if (out.every <= 0) out.every = out.play ? 0 : 60;
  if (!out.screenshot.empty() && out.shot_frame < 0) out.shot_frame = 120;
  if (out.frames < 0) return false;
  for (const std::string& spec : out.click_specs) {
    Args::ClickAt click;
    if (!parse_click_spec(spec, out.click_frame, click.x, click.y, click.frame)) {
      print_error("[godot] bad --click value: " + spec + " (want <x>,<y>[@<frame>])");
      return false;
    }
    out.clicks.push_back(click);
  }
  for (const std::string& spec : out.window_click_specs) {
    Args::ClickAt click;
    if (!parse_click_spec(spec, out.click_frame, click.x, click.y, click.frame)) {
      print_error("[godot] bad --click-window value: " + spec + " (want <x>,<y>[@<frame>])");
      return false;
    }
    click.window_space = true;
    out.clicks.push_back(click);
  }
  for (const std::string& spec : out.hover_specs) {
    Args::ClickAt click;
    if (!parse_click_spec(spec, out.click_frame, click.x, click.y, click.frame)) {
      print_error("[godot] bad --hover-window value: " + spec + " (want <x>,<y>[@<frame>])");
      return false;
    }
    click.window_space = true;
    click.hover = true;
    out.clicks.push_back(click);
  }
  for (const std::string& spec : out.drag_specs) {
    std::string text = spec;
    int frame = out.click_frame;
    const std::size_t at = text.rfind('@');
    if (at != std::string::npos) {
      frame = std::atoi(text.c_str() + at + 1);
      text.resize(at);
    }
    Args::DragAt drag;
    drag.frame = frame;
    if (std::sscanf(text.c_str(), "%lf,%lf,%lf,%lf", &drag.x0, &drag.y0, &drag.x1, &drag.y1) != 4 ||
        frame < 0) {
      print_error("[godot] bad --drag-window value: " + spec +
                  " (want <x0>,<y0>,<x1>,<y1>[@<frame>])");
      return false;
    }
    out.drags.push_back(drag);
  }
  for (const Args::KeySpec& spec : out.key_specs) {
    const size_t at = spec.text.find('@');
    const std::string name = at == std::string::npos ? spec.text : spec.text.substr(0, at);
    int frame = out.key_frame;
    if (at != std::string::npos) frame = std::atoi(spec.text.substr(at + 1).c_str());
    if (name.empty() || frame < 0) {
      print_error("[godot] bad --key value: " + spec.text + " (want <name>[@<frame>])");
      return false;
    }
    const std::u16string key = dom_key_of_name(name);
    if (spec.kind == "tap") {
      out.keys.push_back(Args::KeyAt{frame, true, key});
      out.keys.push_back(Args::KeyAt{frame + out.key_tap, false, key});
    } else {
      out.keys.push_back(Args::KeyAt{frame, spec.kind == "down", key});
    }
  }
  std::stable_sort(out.keys.begin(), out.keys.end(),
                   [](const Args::KeyAt& a, const Args::KeyAt& b) { return a.frame < b.frame; });
  return (!out.zips.empty() || !out.remote_zips.empty() || !out.mod_zips.empty()) &&
         (out.play || out.frames > 0);
}

}
