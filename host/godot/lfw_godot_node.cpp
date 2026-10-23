#include "host/godot/lfw_godot_node.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
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
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/cmd.h"
#include "lfw/entity/gen_field.h"
#include "lfw/ui/action/actor.h"
#include "lfw/ui/ui_action_enum.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

namespace lfw::host {
namespace {

using godot::String;
using godot::UtilityFunctions;
using godot::Color;
using godot::Rect2;
using godot::Vector2;

std::u16string from_utf8(const std::string& s) {
  return decode_utf8(std::vector<uint8_t>(s.begin(), s.end()));
}

std::string to_utf8(const std::u16string& s) {
  const std::vector<uint8_t> buf = encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

double num_field(const Value& v, double fallback) {
  if (std::holds_alternative<std::monostate>(v)) return fallback;
  const double n = to_number(v);
  return std::isnan(n) ? fallback : n;
}

void print_line(const std::string& text) { UtilityFunctions::print(String::utf8(text.c_str())); }

void print_error(const std::string& text) { UtilityFunctions::printerr(String::utf8(text.c_str())); }

bool is_volume_control(int index) {
  const auto& list = lfw::host::titlebar_controls();
  if (index < 0 || index >= static_cast<int>(list.size())) return false;
  const lfw::host::TitleAction action = list[static_cast<std::size_t>(index)].action;
  return action == lfw::host::TitleAction::bgm_mute || action == lfw::host::TitleAction::sound_mute;
}

bool read_file_bytes(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !out.empty();
}

std::filesystem::path find_repo_root(std::filesystem::path start) {
  std::error_code ec;
  for (int depth = 0; depth < 8; ++depth) {
    if (start.empty()) break;
    if (std::filesystem::is_regular_file(start / "src" / "assets" / "btn_1_1.png", ec)) {
      return start;
    }
    const std::filesystem::path parent = start.parent_path();
    if (parent == start) break;
    start = parent;
  }
  return {};
}

std::filesystem::path exe_dir() {
  godot::OS* const os = godot::OS::get_singleton();
  if (os == nullptr) return {};
  const godot::String path = os->get_executable_path();
  if (path.is_empty()) return {};
  return std::filesystem::path(path.utf8().get_data()).parent_path();
}

/**
 * 打包版布局：`<exe 所在目录>/resources/{root,prel,data,icons}`。
 * 存在就返回它；开发时直接跑 Godot 二进制，那个目录不存在，返回空。
 */
std::filesystem::path packaged_resources_dir() {
  const std::filesystem::path dir = exe_dir() / "resources";
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return {};
  return dir;
}

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

std::string lower_ascii(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

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

/** 自定义游戏包：包内根级 `index.json` / `index.json5` 指向包内其它 zip（至少 2 个） */
bool read_full_game_index(IZip& zip, std::vector<std::u16string>& paths,
                          std::shared_ptr<lfw::Object>& info) {
  IZipObject* index = zip.file(u"index.json");
  if (index == nullptr) index = zip.file(u"index.json5");
  if (index == nullptr) return false;
  Value parsed;
  std::u16string error;
  if (!index->json(parsed, error)) return false;
  const std::u16string name = zip.name();
  info = std::make_shared<lfw::Object>();
  info->set(u"type", Value(std::u16string(u"FULL")));
  info->set(u"title", Value(name));
  info->set(u"version", Value(0.0));
  info->set(u"description", Value(name));
  info->set(u"author", Value(std::u16string(u"")));
  const auto collect = [&paths](const Array& arr) {
    for (size_t i = 0; i < arr.size(); ++i) {
      if (const std::u16string* const s = std::get_if<std::u16string>(&arr.at(i)))
        paths.push_back(*s);
    }
  };
  if (const Array* const arr = as_array(parsed); arr != nullptr) {
    collect(*arr);  // 老格式：直接就是路径数组
  } else if (const Object* const obj = as_object(parsed); obj != nullptr) {
    const Value* const type = obj->get(u"type");
    const std::u16string* const tag = type != nullptr ? std::get_if<std::u16string>(type) : nullptr;
    if (tag == nullptr || *tag != u"FULL") return false;
    const Value* const version = obj->get(u"version");
    if (version == nullptr || !std::holds_alternative<double>(*version)) return false;
    info->set(u"version", *version);
    for (const char16_t* key : {u"title", u"description", u"author"}) {
      const Value* const v = obj->get(key);
      if (v != nullptr && std::holds_alternative<std::u16string>(*v)) info->set(key, *v);
    }
    const Value* const list = obj->get(u"paths");
    if (const Array* const arr = list != nullptr ? as_array(*list) : nullptr; arr != nullptr)
      collect(*arr);
  } else {
    return false;
  }
  if (paths.size() < 2) return false;
  auto arr = std::make_shared<Array>();
  for (const std::u16string& p : paths) arr->push_back(Value(p));
  info->set(u"paths", Value(std::move(arr)));
  return true;
}

/** 用户数据目录（Godot 的 `user://`）下的 `mods/` */
std::filesystem::path user_mods_dir() {
  godot::OS* const os = godot::OS::get_singleton();
  if (os == nullptr) return {};
  const godot::String dir = os->get_user_data_dir();
  if (dir.is_empty()) return {};
  return std::filesystem::path(dir.utf8().get_data()) / "mods";
}

/**
 * 「自定义游戏」选好的包暂存到这里：重启后优先加载，加载完就删 ——
 * 只对本次会话生效（下次正常启动还是默认数据）。
 */
std::filesystem::path pending_custom_game_file() {
  godot::OS* const os = godot::OS::get_singleton();
  if (os == nullptr) return {};
  const godot::String dir = os->get_user_data_dir();
  if (dir.is_empty()) return {};
  return std::filesystem::path(dir.utf8().get_data()) / "custom-game" / "pending.zip";
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

std::string cwd_string() {
  std::error_code ec;
  const std::filesystem::path cwd = std::filesystem::current_path(ec);
  return ec ? std::string("<unknown>") : cwd.generic_string();
}

const char* titlebar_name(LfwGodot::TitlebarMode mode) {
  switch (mode) {
    case LfwGodot::TitlebarMode::native:
      return "native";
    case LfwGodot::TitlebarMode::none:
      return "none";
    case LfwGodot::TitlebarMode::custom:
      return "custom";
    case LfwGodot::TitlebarMode::frame:
      return "frame";
  }
  return "?";
}

godot::DisplayServer::WindowResizeEdge to_window_edge(TitleEdge edge) {
  switch (edge) {
    case TitleEdge::top_left:
      return godot::DisplayServer::WINDOW_EDGE_TOP_LEFT;
    case TitleEdge::top:
      return godot::DisplayServer::WINDOW_EDGE_TOP;
    case TitleEdge::top_right:
      return godot::DisplayServer::WINDOW_EDGE_TOP_RIGHT;
    case TitleEdge::left:
      return godot::DisplayServer::WINDOW_EDGE_LEFT;
    case TitleEdge::right:
      return godot::DisplayServer::WINDOW_EDGE_RIGHT;
    case TitleEdge::bottom_left:
      return godot::DisplayServer::WINDOW_EDGE_BOTTOM_LEFT;
    case TitleEdge::bottom:
      return godot::DisplayServer::WINDOW_EDGE_BOTTOM;
    case TitleEdge::bottom_right:
      return godot::DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT;
    case TitleEdge::none:
      break;
  }
  return godot::DisplayServer::WINDOW_EDGE_TOP;
}

std::string window_state_text(const godot::DisplayServer* ds) {
  const godot::Vector2i size = ds->window_get_size();
  const godot::Vector2i deco = ds->window_get_size_with_decorations();
  const godot::Vector2i pos = ds->window_get_position();
  char buf[160];
  std::snprintf(buf, sizeof(buf), "mode=%d size=%dx%d deco=%dx%d pos=%d,%d",
                static_cast<int>(ds->window_get_mode()), size.x, size.y, deco.x, deco.y, pos.x,
                pos.y);
  return buf;
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
         "  --root     import 根目录（含 builtin_data/…），可重复；缺省 `.`\n"
         "  --zip      数据包：解包目录或 `.zip` 文件，可重复；按给定顺序 load\n"
         "  --mods     额外模组目录（放 *.zip 的目录），可重复；比默认目录后加载 = 优先级更高\n"
         "  --drop     诊断：启动后按「拖拽」路径把一个 .zip 当模组热加载（和窗口里拖入等价），可重复\n"
         "  --custom-game  诊断：启动后走「自定义游戏」路径（暂存 + 重启）加载一个 full.zip，可重复\n"
         "  --no-mods  不自动加载模组（默认目录与 `--mods` 都跳过）\n"
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
         "  --text-input  脚本化打字：`<text>[@<frame>]`。第 `frame` 帧让文本输入覆盖层（Web 的\n"
         "                DOM `<input>` 在 Godot 侧的等价物：一个真的 `LineEdit`）拿焦点并整串写入，\n"
         "                `+2` 帧放掉焦点（⇒ 组件 `on_blur` 存盘），`+4` 帧打一行事实。缺省帧 60。\n"
         "  打包版（免安装目录）：不带任何参数时等价于 `-- --play`，`--root/--zip/--icons`\n"
         "             自动取 `<exe 所在目录>/resources/{root,prel,data,icons}`，\n"
         "             `<exe 所在目录>/mods/*.zip` 自动作为模组加载。\n";
}

bool parse_click_spec(const std::string& text, int default_frame, double& x, double& y, int& frame) {
  const size_t at = text.find('@');
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
  for (size_t i = 0; i < argv.size(); ++i) {
    const std::string& a = argv[i];
    std::string v;
    if (a == "--root") {
      if (!need(i, v)) return false;
      out.roots.emplace_back(v);
    } else if (a == "--zip") {
      if (!need(i, v)) return false;
      out.zips.emplace_back(v);
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
    } else if (a == "--display-scale") {
      if (!need(i, v)) return false;
      out.display_scale = std::strtod(v.c_str(), nullptr);
      if (!(out.display_scale > 0.0)) return false;
    } else if (a == "--ui-page") {
      if (!need(i, v)) return false;
      out.ui_page = from_utf8(v);
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
  return (!out.zips.empty() || !out.mod_zips.empty()) && (out.play || out.frames > 0);
}

void LfwGodot::_ready() {
  std::vector<std::string> argv = user_args();
  _args = Args{};
  if (argv.empty()) {
    // 打包版：零参数 = `--play`，资源从 `<exe>/resources` 自动补齐；
    // 没有那个目录（开发时直接跑 Godot 二进制）就还是什么都不做。
    if (packaged_resources_dir().empty()) {
      print_line("[godot] no LFW arguments after `--`; nothing to do.");
      print_line(usage());
      return;
    }
    argv.emplace_back("--play");
    print_line("[godot] packaged build: resources auto-detected next to the executable");
  }
  if (argv.size() == 1 && (argv[0] == "--help" || argv[0] == "-h")) {
    print_line(usage());
    return;
  }
  if (!parse_args(argv, _args)) {
    print_error("[godot] bad arguments");
    print_line(usage());
    quit_with(64);
    return;
  }
  if (godot::ProjectSettings* const settings = godot::ProjectSettings::get_singleton()) {
    _logical_w = static_cast<int>(settings->get_setting("display/window/size/viewport_width", _logical_w));
    _logical_h =
        static_cast<int>(settings->get_setting("display/window/size/viewport_height", _logical_h));
  }
  apply_window_setup();
  // 窗口标题在 `_process` 第一帧统一处理（Godot 启动后还会覆盖一次）
  // 像素画：整张画布用最近邻采样（项目设置里也写了同一档，这里再显式设一次，
  // 免得别处的默认值把它覆盖掉）。文字走字体 oversampling，仍是 1:1 光栅化。
  set_texture_filter(godot::CanvasItem::TEXTURE_FILTER_NEAREST);
  setup_icons();
  if (!setup()) {
    quit_with(1);
    return;
  }
  set_process_input(true);
  _running = true;
  if (titlebar_visible()) report_titlebar_controls();
  // 拖拽加载：系统把文件拖进窗口时 Godot 发 `files_dropped`
  if (godot::Window* const win = get_window(); win != nullptr) {
    win->connect("files_dropped", callable_mp(this, &LfwGodot::_on_files_dropped));
    print_line("[godot] drop: drag a .zip into the window to load it as a mod");
    if (!_args.drop_zips.empty()) {
      // `--drop`：走同一条信号链（诊断/验收用）
      godot::PackedStringArray files;
      for (const std::filesystem::path& path : _args.drop_zips) {
        files.push_back(godot::String(path.generic_string().c_str()));
      }
      win->emit_signal("files_dropped", files);
    }
  }
  // `--custom-game`：走「自定义游戏」同一条路径（暂存 + 重启），诊断/验收用
  for (const std::filesystem::path& path : _args.custom_games) switch_to_full_game(path);
}

void LfwGodot::_on_files_dropped(const godot::PackedStringArray& files) {
  std::vector<std::filesystem::path> paths;
  for (int64_t i = 0; i < files.size(); ++i) {
    paths.emplace_back(files[i].utf8().get_data());
  }
  load_dropped(paths);
}

void LfwGodot::open_mod_dialog() {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return;
  if (!ds->has_feature(godot::DisplayServer::FEATURE_NATIVE_DIALOG)) {
    print_error("[godot] mod dialog: 当前平台没有原生文件对话框（改用把 zip 拖进窗口 / mods/）");
    return;
  }
  godot::PackedStringArray filters;
  filters.push_back("*.zip ; ZIP");
  const godot::Callable cb = callable_mp(this, &LfwGodot::_on_mod_files_selected);
  const godot::Window* const win = get_window();
  const int32_t window_id = win != nullptr ? win->get_window_id() : 0;
  print_line("[godot] mod dialog: open (pick *.zip)");
  ds->file_dialog_show("选择模组 zip", "", "", false, godot::DisplayServer::FILE_DIALOG_MODE_OPEN_FILES,
                       filters, cb, window_id);
}

void LfwGodot::_on_mod_files_selected(bool status, const godot::PackedStringArray& paths) {
  if (!status) {
    print_line("[godot] mod dialog: cancelled");
    return;
  }
  std::vector<std::filesystem::path> files;
  for (int64_t i = 0; i < paths.size(); ++i) {
    files.emplace_back(paths[i].utf8().get_data());
  }
  load_dropped(files);
}

void LfwGodot::open_custom_game_dialog() {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return;
  if (!ds->has_feature(godot::DisplayServer::FEATURE_NATIVE_DIALOG)) {
    print_error("[godot] custom game: 当前平台没有原生文件对话框（改用把 full.zip 拖进窗口）");
    return;
  }
  godot::PackedStringArray filters;
  filters.push_back("*.zip ; ZIP");
  const godot::Callable cb = callable_mp(this, &LfwGodot::_on_custom_game_selected);
  const godot::Window* const win = get_window();
  const int32_t window_id = win != nullptr ? win->get_window_id() : 0;
  print_line("[godot] custom game: pick a full.zip (contains index.json)");
  ds->file_dialog_show("选择自定义游戏包（full.zip）", "", "", false,
                       godot::DisplayServer::FILE_DIALOG_MODE_OPEN_FILE, filters, cb, window_id);
}

void LfwGodot::_on_custom_game_selected(bool status, const godot::PackedStringArray& paths) {
  if (!status || paths.is_empty()) return;
  switch_to_full_game(std::filesystem::path(paths[0].utf8().get_data()));
}

bool LfwGodot::switch_to_full_game(const std::filesystem::path& path) {
  std::unique_ptr<ZipFile> archive;
  std::u16string error;
  if (!ZipFile::open_cached(from_utf8(path.filename().generic_string()), path, std::nullopt, archive,
                            error)) {
    print_error("[godot] custom game: " + to_utf8(error));
    return false;
  }
  std::vector<std::u16string> inner_paths;
  std::shared_ptr<lfw::Object> info;
  if (!read_full_game_index(*archive, inner_paths, info)) {
    print_error("[godot] custom game: 不是自定义游戏包（根目录缺 index.json / index.json5，或 paths 少于 2 个）: " +
                path.filename().generic_string());
    return false;
  }
  // 整包替换运行中做不到：暂存一份，重启新进程时优先加载（加载完就删，只对本会话生效）
  const std::filesystem::path pending = pending_custom_game_file();
  if (pending.empty()) {
    print_error("[godot] custom game: 拿不到用户数据目录");
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(pending.parent_path(), ec);
  std::filesystem::copy_file(path, pending, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    print_error("[godot] custom game: copy failed: " + ec.message());
    return false;
  }
  godot::OS* const os = godot::OS::get_singleton();
  if (os == nullptr) return false;
  print_line("[godot] custom game: restarting with " + path.filename().generic_string());
  // 重启命令：`--path <工程目录> --` + 原来的用户参数（摘掉一次性的 --drop / --custom-game，
  // 免得子进程重复执行同一个「换包」请求）。`get_cmdline_args()` 拿不到引擎已消费的参数，
  // 所以工程路径从 `res://` 反查。
  godot::PackedStringArray args;
  if (godot::ProjectSettings* const settings = godot::ProjectSettings::get_singleton()) {
    const godot::String project_dir = settings->globalize_path("res://");
    if (!project_dir.is_empty()) {
      args.push_back("--path");
      args.push_back(project_dir);
    }
  }
  args.push_back("--");
  const godot::PackedStringArray user_args = os->get_cmdline_user_args();
  std::string joined;
  for (int64_t i = 0; i < user_args.size(); ++i) {
    const godot::String& a = user_args[i];
    if (a == "--drop" || a == "--custom-game") {
      ++i;  // 连同它的值一起丢
      continue;
    }
    args.push_back(a);
    if (!joined.empty()) joined += " ";
    joined += a.utf8().get_data();
  }
  const int32_t pid = os->create_process(os->get_executable_path(), args, false);
  if (pid <= 0) {
    print_error("[godot] custom game: restart failed (pid=" + std::to_string(pid) + ")");
    return false;
  }
  print_line("[godot] custom game: spawned pid=" + std::to_string(pid) +
             " user_args=" + joined);
  quit_with(0);
  return true;
}

bool LfwGodot::open_full_pack(ZipFile& zip, const std::string& label,
                              const std::vector<std::u16string>& paths,
                              std::vector<IZip*>& out) {
  std::vector<std::unique_ptr<ZipFile>> opened;
  std::vector<std::string> names;
  for (const std::u16string& inner : paths) {
    ZipFileObject* const entry = dynamic_cast<ZipFileObject*>(zip.file(inner));
    std::string inner_raw;
    std::u16string error;
    if (entry == nullptr || !entry->read(inner_raw, error)) {
      print_error("[godot] custom game pack: missing " + to_utf8(inner) + " in " + label);
      return false;
    }
    std::vector<uint8_t> bytes(inner_raw.begin(), inner_raw.end());
    std::unique_ptr<ZipFile> archive;
    if (!ZipFile::open(inner, std::move(bytes), std::nullopt, archive, error)) {
      print_error("[godot] custom game pack: " + to_utf8(error));
      return false;
    }
    names.push_back(to_utf8(inner));
    opened.push_back(std::move(archive));
  }
  for (size_t i = 0; i < opened.size(); ++i) {
    _archives.push_back(std::move(opened[i]));
    out.push_back(_archives.back().get());
    char buf[300];
    std::snprintf(buf, sizeof(buf), "[godot] package: %s/%s (%zu entries, zip)", label.c_str(),
                  names[i].c_str(), _archives.back()->entry_count());
    print_line(buf);
  }
  return true;
}

void LfwGodot::load_dropped(const std::vector<std::filesystem::path>& files) {
  if (_lfw == nullptr) return;
  // 已经加载过的同名包不重复加载
  std::set<std::u16string> seen;
  for (const lfw::LFW::ZipItem& it : LFW::ZIPS()) {
    if (it.zip != nullptr) seen.insert(it.zip->name());
  }
  std::vector<lfw::LFW::ZipItem> items;
  std::string loaded_names;
  for (const std::filesystem::path& path : files) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
      print_error("[godot] drop: not a file: " + path.generic_string());
      continue;
    }
    std::string name = path.filename().string();
    if (name.size() <= 4 || lower_ascii(name.substr(name.size() - 4)) != ".zip") {
      print_line("[godot] drop: ignored (not a zip): " + name);
      continue;
    }
    const std::u16string uname = from_utf8(name);
    if (seen.find(uname) != seen.end()) {
      print_line("[godot] drop: already loaded: " + name);
      continue;
    }
    std::unique_ptr<ZipFile> archive;
    std::u16string zip_error;
    if (!ZipFile::open_cached(uname, path, std::nullopt, archive, zip_error)) {
      print_error("[godot] drop: " + to_utf8(zip_error));
      continue;
    }
    // 自定义游戏包 = 整包替换本体数据：暂存 + 重启（运行中换不了数据源）
    std::vector<std::u16string> inner_paths;
    std::shared_ptr<lfw::Object> info;
    if (read_full_game_index(*archive, inner_paths, info)) {
      print_line("[godot] drop: " + name + " 是自定义游戏包，重启后加载");
      switch_to_full_game(path);
      continue;
    }
    _archives.push_back(std::move(archive));
    items.push_back(lfw::LFW::ZipItem{u"", _archives.back().get()});
    seen.insert(uname);
    if (!loaded_names.empty()) loaded_names += ", ";
    loaded_names += name;
  }
  if (items.empty()) return;

  std::u16string error;
  if (!_lfw->load(items, error)) {
    print_error("[godot] drop: load failed: " + to_utf8(error));
    return;
  }
  // 「额外数据」列表跟上（原有 ZIPS + 这次新加的）
  std::vector<lfw::LFW::ZipItem> all = LFW::ZIPS();
  for (const lfw::LFW::ZipItem& it : items) all.push_back(it);
  LFW::set_ZIPS(all);
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "[godot] drop: loaded %zu (%s); data now fighters=%zu weapons=%zu balls=%zu "
                "entities=%zu stages=%zu bg=%zu bots=%zu moves=%zu directors=%zu",
                items.size(), loaded_names.c_str(), _lfw->datas().fighters().size(),
                _lfw->datas().weapons().size(), _lfw->datas().balls().size(),
                _lfw->datas().entities().size(), _lfw->datas().stages().size(),
                _lfw->datas().backgrounds().size(), _lfw->datas().bots().size(),
                _lfw->datas().moves().size(), _lfw->datas().directors().size());
  print_line(buf);
}

bool LfwGodot::setup() {
  _host = std::make_unique<LfwGodotHost>(_args.roots, false);
  install_text_metrics();
  install_audio();
  lfw::set_clock(&_host->clock());
  lfw::set_timeout(&_host->timeout());
  _host->clock().set(_args.time);

  std::vector<lfw::LFW::ZipItem> items;
  // 打开一个 `.zip`：stored/deflate 按需读（整包不进内存）；其它方法先解压到缓存再按需读
  const auto open_zip = [this](const std::filesystem::path& path, const std::string& label) -> ZipFile* {
    std::unique_ptr<ZipFile> archive;
    std::u16string zip_error;
    // 名字取文件名：UI 的「额外数据」列的是 `IZip::name()`
    if (!ZipFile::open_cached(from_utf8(path.filename().generic_string()), path, std::nullopt,
                              archive, zip_error)) {
      print_error("[godot] bad package (" + path.generic_string() + "): " + to_utf8(zip_error));
      return nullptr;
    }
    if (archive->needs_extract()) {
      print_line("[godot] package: " + path.filename().generic_string() + " 含内建不支持的方法（" +
                 archive->unsupported_methods() + "），使用解压缓存 " +
                 archive->extracted_root().generic_string());
    }
    _archives.push_back(std::move(archive));
    char buf[300];
    std::snprintf(buf, sizeof(buf), "[godot] package: %s (%zu entries, zip)", label.c_str(),
                  _archives.back()->entry_count());
    print_line(buf);
    return _archives.back().get();
  };
  // 数据包：目录或 `.zip`
  const auto open_package = [this, &open_zip](const std::filesystem::path& path) -> IZip* {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
      _zips.push_back(std::make_unique<FsZip>(from_utf8(path.generic_string()), path));
      char buf[256];
      std::snprintf(buf, sizeof(buf), "[godot] package: %s (%zu entries)",
                    path.generic_string().c_str(), _zips.back()->entry_count());
      print_line(buf);
      return _zips.back().get();
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
      print_error("[godot] not a directory or zip: " + path.generic_string() + " (cwd=" +
                  cwd_string() + ")");
      return nullptr;
    }
    return open_zip(path, path.generic_string());
  };

  // 0) 「自定义游戏」选好的包（上一进程写到 user://custom-game/pending.zip）：优先用它
  std::vector<IZip*> pending_inner;
  std::shared_ptr<lfw::Object> pending_info;
  {
    const std::filesystem::path pending = pending_custom_game_file();
    std::error_code pending_ec;
    if (!pending.empty() && std::filesystem::is_regular_file(pending, pending_ec)) {
      ZipFile* const zip = open_zip(pending, "custom-game (pending)");
      std::vector<std::u16string> inner_paths;
      if (zip != nullptr && read_full_game_index(*zip, inner_paths, pending_info) &&
          open_full_pack(*zip, "custom-game", inner_paths, pending_inner)) {
        print_line("[godot] mods: custom game pack (pending) replaces the base data");
      } else {
        print_error("[godot] mods: 待加载的自定义游戏包无效，已忽略");
        pending_inner.clear();
      }
      std::filesystem::remove(pending, pending_ec);  // 只对本次会话生效
    }
  }

  // 1) 模组：内含 index.json 的是「自定义游戏包」，会整包替换本体数据
  std::vector<IZip*> mod_packs;
  std::vector<IZip*> full_inner;
  std::shared_ptr<lfw::Object> full_info;
  std::string full_name;
  for (const std::filesystem::path& path : _args.mod_zips) {
    ZipFile* const zip = open_zip(path, path.generic_string());
    if (zip == nullptr) continue;
    std::vector<std::u16string> inner_paths;
    std::shared_ptr<lfw::Object> info;
    if (!read_full_game_index(*zip, inner_paths, info)) {
      mod_packs.push_back(zip);
      continue;
    }
    if (!pending_inner.empty() || !full_inner.empty()) {
      print_line("[godot] mods: only the first custom game pack is used, ignoring " +
                 path.filename().generic_string());
      continue;
    }
    std::vector<IZip*> opened;
    if (!open_full_pack(*zip, path.filename().generic_string(), inner_paths, opened)) {
      mod_packs.push_back(zip);  // 解析失败就当普通数据包
      continue;
    }
    full_inner = std::move(opened);
    full_info = std::move(info);
    full_name = path.filename().generic_string();
  }

  if (!pending_inner.empty()) {
    LFW::INFO() = Value(pending_info);
    for (IZip* const zip : pending_inner) items.push_back(lfw::LFW::ZipItem{u"", zip});
  } else if (!full_inner.empty()) {
    print_line("[godot] mods: custom game pack " + full_name + " replaces the base data");
    LFW::INFO() = Value(full_info);
    for (IZip* const zip : full_inner) items.push_back(lfw::LFW::ZipItem{u"", zip});
  } else {
    for (const std::filesystem::path& path : _args.zips) {
      if (IZip* const zip = open_package(path)) items.push_back(lfw::LFW::ZipItem{u"", zip});
    }
  }
  for (IZip* const zip : mod_packs) items.push_back(lfw::LFW::ZipItem{u"", zip});

  const std::string version_text = resolve_version_name();
  LFW::VERSION_NAME() = from_utf8(version_text);
  print_line("[godot] version name: " + version_text);

  _lfw = std::make_unique<lfw::LFW>(*_host, false);
  // 「额外数据」列表：ZIPS 就是本次实际加载的包
  LFW::set_ZIPS(items);
  // 入口页「附加模组」/「添加模组」：UI 只发 `select_extra_data` / `select_mods` 广播，
  // 选文件得宿主来做
  _lfw->callbacks.on(u"on_broadcast", [this](const lfw::LfwCallbacks::Payloads& p) {
    if (p.empty()) return;
    const std::u16string& msg = p[0].text;
    if (msg == u"select_extra_data" || msg == u"select_mods") {
      open_mod_dialog();
    } else if (msg == u"custom_game") {
      open_custom_game_dialog();
    }
  });

  ui::actor().add(ui::ui_action::kExit, [this](ui::UINode& node, const std::vector<Value>&) {
    static_cast<void>(node);
    print_line("[godot] ui action: exit");
    print_exit_summary();
    quit_with(0);
  });

  {
    std::u16string lang = _args.lang;
    if (lang.empty()) {
      const godot::String locale = godot::OS::get_singleton()->get_locale();
      std::string s = locale.utf8().get_data();
      for (char& c : s) {
        if (c == '_') c = '-';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      lang = from_utf8(s);
    }
    if (!lang.empty()) {
      _lfw->set_lang(Value(lang));
      print_line("[godot] ui lang: " + to_utf8(lang));
    }
  }

  std::u16string error;
  if (!_lfw->load(items, error)) {
    print_error("[godot] load failed: " + to_utf8(error));
    for (const std::u16string& e : _host->last_errors()) print_error("  - " + to_utf8(e));
    return false;
  }
  {
    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "[godot] loaded: fighters=%zu weapons=%zu balls=%zu entities=%zu stages=%zu "
                  "bg=%zu bots=%zu moves=%zu directors=%zu",
                  _lfw->datas().fighters().size(), _lfw->datas().weapons().size(),
                  _lfw->datas().balls().size(), _lfw->datas().entities().size(),
                  _lfw->datas().stages().size(), _lfw->datas().backgrounds().size(),
                  _lfw->datas().bots().size(), _lfw->datas().moves().size(),
                  _lfw->datas().directors().size());
    print_line(buf);
  }
  // 启动页那行「推荐桌面平台 + 现代浏览器」是给浏览器版看的，原生宿主不需要
  _lfw->hide_ui_nodes({u"desktop_hint"});
  print_line("[godot] ui fonts: " + _fonts.describe());

  if (_args.play) {
    print_line("[godot] play mode: no boot_scene; the entry UI page drives (entry page: init)");
  } else {
    BootOptions boot;
    boot.fighters = _args.fighters;
    boot.stage = _args.stage;
    boot.players = _args.players;
    boot.vs = _args.vs;
    boot.on_spawn = [](const SpawnInfo& s) {
      char buf[320];
      std::snprintf(buf, sizeof(buf), "[godot] spawn p%s %s @(%.1f, %.1f, %.1f) team=%s%s",
                    to_utf8(s.player_id).c_str(), to_utf8(s.fighter_id).c_str(), s.x, s.y, s.z,
                    to_utf8(s.team).c_str(), s.vs ? " (vs)" : "");
      print_line(buf);
    };
    boot.on_failure = [](const BootFailure& f) {
      print_error(f.create_failed ? "[godot] create fighter failed: " + to_utf8(f.fighter_id)
                                  : "[godot] fighter data not found: " + to_utf8(f.fighter_id));
      return true;
    };
    const BootResult booted = boot_scene(*_lfw, boot);
    if (!booted.ok) {
      print_error("[godot] no fighter data in the loaded packages");
      return false;
    }
    char buf[320];
    std::snprintf(buf, sizeof(buf), "[godot] stage=%s bg=%s entities=%zu",
                  to_utf8(booted.stage_id).c_str(), to_utf8(booted.bg_id).c_str(),
                  _lfw->world().entities.size());
    print_line(buf);
  }

  if (!_args.ui_page.empty()) {
    bool found = false;
    for (const Value& v : _lfw->ui_helper().all()) {
      const Value id = lfw::field_or(v, u"id");
      const std::u16string* const s = std::get_if<std::u16string>(&id);
      if (s != nullptr && *s == _args.ui_page) {
        found = true;
        break;
      }
    }
    if (!found) {
      print_error("[godot] --ui-page: no such page id: " + to_utf8(_args.ui_page));
      return false;
    }
    auto opts = std::make_shared<lfw::Object>();
    opts->set(u"id", Value(_args.ui_page));
    _lfw->set_page(Value(std::move(opts)), 0.0);
    print_line("[godot] ui page: " + to_utf8(_args.ui_page));
  }

  lfw::World& world = _lfw->world();
  const double ups = num_field(Value(world.dataset.UPS()), 60.0);
  _host->set_step_ms(1000.0 / (ups > 0 ? ups : 60.0));

  if (_host->world_renderer() != nullptr) {
    _textures = std::make_unique<GodotTextureCache>([this](const std::string& path,
                                                          std::string& out) {
      GodotWorldRenderer* const renderer = _host->world_renderer();
      return renderer != nullptr && renderer->read_resource(path, out);
    });
    _models = std::make_unique<GodotModelBank>([this](const std::string& path, std::string& out) {
      GodotWorldRenderer* const renderer = _host->world_renderer();
      return renderer != nullptr && renderer->read_resource(path, out);
    });
    _models->set_parent(this);
  }
  if (_host->world_renderer() != nullptr) _host->world_renderer()->set_ui_dump(_args.ui_dump);
  return true;
}

void LfwGodot::install_text_metrics() {
  if (_host == nullptr) return;
  godot::ThemeDB* const db = godot::ThemeDB::get_singleton();
  _fonts.set_fallback(db != nullptr ? db->get_fallback_font() : godot::Ref<godot::Font>());
  if (_fonts.fallback().is_null()) {
    print_line("[godot] ui text metrics: no fallback font (零尺寸桩)");
  } else {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "[godot] ui text metrics: fallback font (%s)",
                  _fonts.fallback()->get_font_name().utf8().get_data());
    print_line(buf);
  }
  _host->set_measure_text([this](const Value& text_v, const Value& style_v) -> Value {
    const std::u16string s = lfw::to_string(text_v);
    const std::string utf8 = to_utf8(s);
    CssFont spec;
    spec.family = "Arial";
    spec.size_px = 9;
    const lfw::Object* const style = lfw::as_object(style_v);
    std::string css;
    if (style != nullptr) {
      const Value* const font = style->get(u"font");
      if (font != nullptr) {
        const std::u16string* const f = std::get_if<std::u16string>(font);
        if (f != nullptr && !f->empty()) css = to_utf8(*f);
      }
    }
    if (!css.empty()) {
      const CssFont parsed = parse_css_font(css);
      spec.family = parsed.family.empty() ? spec.family : parsed.family;
      if (parsed.size_px > 0) spec.size_px = parsed.size_px;
      spec.weight = parsed.weight;
      spec.italic = parsed.italic;
    }
    double font_size = spec.size_px > 0 ? spec.size_px : 9;
    const godot::Ref<godot::Font> font = _fonts.get(spec);
    double pad_l = 2.0;
    double pad_r = 2.0;
    double pad_t = 2.0;
    double pad_b = 2.0;
    if (style != nullptr) {
      const auto pad_of = [style](const char16_t* key) {
        const Value* const v = style->get(key);
        if (v == nullptr) return 2.0;
        const double n = to_number(*v);
        return std::isnan(n) ? 2.0 : n;
      };
      pad_l = pad_of(u"padding_l");
      pad_r = pad_of(u"padding_r");
      pad_t = pad_of(u"padding_t");
      pad_b = pad_of(u"padding_b");
    }
    double w = 0.0;
    double h = 0.0;
    if (!font.is_null()) {
      const int size = static_cast<int>(std::lround(font_size));
      const int px = size > 0 ? size : 1;
      std::size_t start = 0;
      int lines = 0;
      while (true) {
        const std::size_t nl = utf8.find('\n', start);
        const std::string line = utf8.substr(start, nl == std::string::npos ? std::string::npos
                                                                            : nl - start);
        const godot::Vector2 sz = font->get_string_size(String::utf8(line.c_str()),
                                                        godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, px);
        if (static_cast<double>(sz.x) > w) w = static_cast<double>(sz.x);
        ++lines;
        if (nl == std::string::npos) break;
        start = nl + 1;
      }
      h = static_cast<double>(font->get_height(px)) * static_cast<double>(lines);
      w += pad_l + pad_r;
      h += pad_t + pad_b;
    }
    auto info = std::make_shared<lfw::Object>();
    info->set(u"text", text_v);
    info->set(u"style", style_v);
    info->set(u"w", Value(w));
    info->set(u"h", Value(h));
    info->set(u"scale", Value(1.0));
    return Value(info);
  });
}

godot::Ref<godot::Font> LfwGodot::font_for_css(const std::string& css) {
  CssFont spec;
  spec.family = "Arial";
  spec.size_px = 9;
  if (!css.empty()) {
    const CssFont parsed = parse_css_font(css);
    if (!parsed.family.empty()) spec.family = parsed.family;
    if (parsed.size_px > 0) spec.size_px = parsed.size_px;
    spec.weight = parsed.weight;
    spec.italic = parsed.italic;
  }
  return _fonts.get(spec);
}

std::string LfwGodot::resolve_version_name() const {
  if (!_args.version_name.empty()) return _args.version_name;
  for (const std::filesystem::path& root : _args.roots) {
    const std::filesystem::path file = root / "version.json";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) continue;
    std::ifstream in(file, std::ios::binary);
    if (!in) continue;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::optional<Value> parsed = json_parse(from_utf8(text));
    if (!parsed.has_value()) continue;
    const lfw::Object* const obj = lfw::as_object(*parsed);
    if (obj == nullptr) continue;
    const auto field = [obj](const char16_t* key) -> std::string {
      const Value* const v = obj->get(key);
      if (v == nullptr) return std::string();
      const std::u16string* const s = std::get_if<std::u16string>(v);
      if (s != nullptr) return to_utf8(*s);
      return to_utf8(lfw::to_string(*v));
    };
    const std::string version = field(u"version");
    if (version.empty()) continue;
    std::string name = "v" + version;    const std::string commit = field(u"commit");
    if (!commit.empty()) name += "-" + commit.substr(0, 7);
    if (field(u"dirty") == "true") name += "-dirty";
    const std::string time = field(u"time");
    if (!time.empty()) name += " " + time;
    return name;
  }
  return "v0.0.0";
}

void LfwGodot::report_key_focus(const std::u16string& key) {
  if (!_args.play || _lfw == nullptr) return;
  const ui::UINode* focused = nullptr;
  const auto walk = [&focused](const ui::UINode* n, const auto& self) -> void {
    if (n == nullptr || focused != nullptr) return;
    if (n->focused()) {
      focused = n;
      return;
    }
    for (ui::UINode* const c : n->children()) {
      self(c, self);
      if (focused != nullptr) return;
    }
  };
  for (ui::UINode* const layer : _lfw->layer_nodes()) {
    if (layer == nullptr) continue;
    walk(layer, walk);
    if (focused != nullptr) break;
  }
  std::string id = "(none)";
  bool has_click = false;
  if (focused != nullptr) {
    const Value& data = focused->data();
    if (const Value* const raw_id = ui::field_of(data, u"id")) {
      if (const std::u16string* const s = std::get_if<std::u16string>(raw_id)) id = to_utf8(*s);
    }
    const Value* const actions = ui::field_of(data, u"actions");
    if (actions != nullptr) {
      if (const Value* const click = ui::field_of(*actions, u"click")) has_click = truthy(*click);
    }
  }
  print_line("[godot] keyfocus: key=" + to_utf8(key) + " focused=" + id +
             " click=" + (has_click ? "yes" : "no") + " pages=" + ui_pages_text());
}

void LfwGodot::_process(double p_delta) {
  if (!_running) return;
  // 窗口标题：引擎在 `_ready` 之后还会用 project name + " (DEBUG)" 覆盖一次，
  // 这里在第一帧改回来（`Window::set_title` 会同步到 DisplayServer）
  if (_frames == 0 && !_title_fixed) {
    _title_fixed = true;
    // Godot 给非导出运行的窗口标题追加 " (DEBUG)"（`window_set_title` 里也会加），
    // 所以最后用 Win32 直接改窗口文本
    if (godot::Window* const win = get_window()) win->set_title("Legacy Flame War");
    if (godot::DisplayServer* const ds = godot::DisplayServer::get_singleton()) {
      const int64_t handle = ds->window_get_native_handle(godot::DisplayServer::WINDOW_HANDLE, 0);
      if (handle != 0) {
        lfw::host::set_window_title(reinterpret_cast<void*>(static_cast<intptr_t>(handle)),
                                    "Legacy Flame War");
      }
    }
    apply_window_icon();
  }
  audio_poll();
  if (_capture_pending) {
    _capture_pending = false;
    capture_screenshot();
    finish();
    return;
  }
  const bool real_time =
      _args.frames <= 0 || godot::DisplayServer::get_singleton()->get_name() != "headless";
  double pacing = real_time ? p_delta : (_args.delta > 0.0 ? _args.delta : p_delta);
  if (!(pacing > 0.0)) pacing = 1.0 / 60.0;
  _host->accumulate(pacing);

  const int shot_at = _args.shot_frame > 0 ? _args.shot_frame : -1;
  bool stepped = false;
  while (_running && (unlimited() || _frames < _args.frames) && _host->can_step()) {
    GodotWorldRenderer* const renderer = _host->world_renderer();
    const long long before = renderer != nullptr ? renderer->renders() : 0;
    const int frame = _frames;
    Args::ClickAt* const clicked = inject_clicks(frame);
    const Args::KeyAt* const keyed = inject_keys(frame);
    _host->push_frame(_lfw->world());
    render_world_frame(before);
    sync_text_input();
    probe_text_input(frame);
    stepped = true;
    ++_frames;
    if (clicked != nullptr) {
      char buf[256];
      std::snprintf(buf, sizeof(buf), "[godot] %s @frame %d at=(%.1f, %.1f) pages=%s",
                    clicked->hover ? "hover-win" : (clicked->window_space ? "click-win" : "click"),
                    frame, clicked->x, clicked->y, ui_pages_text().c_str());
      print_line(buf);
    }
    if (keyed != nullptr) {
      const auto pressed = _host->pressed_keys().find(keyed->key);
      char buf[256];
      std::snprintf(buf, sizeof(buf), "[godot] key @frame %d %s %s times=%d pages=%s", frame,
                    keyed->down ? "down" : "up", to_utf8(keyed->key).c_str(),
                    pressed == _host->pressed_keys().end() ? 0 : pressed->second,
                    ui_pages_text().c_str());
      print_line(buf);
    }
    if (_args.every > 0 && (frame % _args.every == 0 || frame == _args.frames - 1)) {
      report_frame(frame);
    }
    if (shot_at >= 0 && _frames >= shot_at) {
      _capture_pending = true;
      break;
    }
  }
  if (stepped) queue_redraw();
  if (_capture_pending) return;
  if (!unlimited() && _frames >= _args.frames) finish();
}

void LfwGodot::render_world_frame(long long renders_before) {
  lfw::World& world = _lfw->world();
  GodotWorldRenderer* const renderer = _host->world_renderer();
  if (renderer == nullptr) return;
  if (renderer->renders() == renders_before) world.render_once(_host->step_ms());
  const GodotFrame& frame = renderer->take_frame();
  if (_models != nullptr) {
    _model_textures.clear();
    for (const GodotDrawItem& item : frame.items) {
      if (!item.has_model) continue;
      godot::Ref<godot::ViewportTexture> tex = _models->sync(item.tag, item.model);
      if (!tex.is_null()) _model_textures[item.tag] = tex;
    }
    _models->end_frame();
  }
}

void LfwGodot::sync_text_input() {
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  if (renderer == nullptr) {
    destroy_text_input();
    return;
  }
  const std::vector<GodotTextInputBox>& boxes = renderer->ui_text_inputs();
  if (boxes.empty()) {
    destroy_text_input();
    return;
  }

  const godot::Transform2D to_canvas =
      final_transform().affine_inverse() * contain_transform(window_size_px());
  const double scale_y = std::fabs(static_cast<double>(to_canvas.get_scale().y));

  std::vector<TextInputOverlay> next;
  next.reserve(boxes.size());
  for (const GodotTextInputBox& box : boxes) {
    TextInputOverlay* overlay = nullptr;
    for (TextInputOverlay& old : _text_inputs) {
      if (old.edit == nullptr || old.node != box.node) continue;
      next.push_back(std::move(old));
      old.edit = nullptr;
      old.node = nullptr;
      overlay = &next.back();
      break;
    }
    if (overlay == nullptr) {
      TextInputOverlay created;
      created.edit = memnew(godot::LineEdit);
      char name_buf[48];
      std::snprintf(name_buf, sizeof(name_buf), "lfw_text_input_%d",
                    static_cast<int>(next.size() + 1));
      created.edit->set_name(name_buf);
      created.edit->set_mouse_filter(godot::Control::MOUSE_FILTER_STOP);
      godot::Ref<godot::StyleBoxEmpty> flat;
      flat.instantiate();
      for (const char* const state : {"normal", "focus", "read_only"}) {
        created.edit->add_theme_stylebox_override(state, flat);
      }
      add_child(created.edit);
      created.node = box.node;
      created.shown = box.input->text();
      created.edit->set_text(String::utf8(to_utf8(created.shown).c_str()));
      next.push_back(std::move(created));
      overlay = &next.back();
    }

    const std::optional<double> max_len = box.input->max_length();
    const int font_px = std::max(
        1, static_cast<int>(std::lround(box.font_size * (scale_y > 0.0 ? scale_y : 1.0))));
    char sig[320];
    std::snprintf(sig, sizeof(sig), "%s|%.3f|%.3f,%.3f,%.3f,%.3f|%d|%d|%d", box.font_css.c_str(),
                  box.font_size, box.r, box.g, box.b, box.a, box.text_align, font_px,
                  max_len.has_value() ? static_cast<int>(*max_len) : -1);
    if (std::string(sig) != overlay->style_sig) {
      overlay->style_sig = sig;
      godot::Ref<godot::Font> font = font_for_css(box.font_css);
      if (!font.is_null()) overlay->edit->add_theme_font_override("font", font);
      overlay->edit->add_theme_font_size_override("font_size", font_px);
      const godot::Color tint(static_cast<float>(box.r), static_cast<float>(box.g),
                              static_cast<float>(box.b), static_cast<float>(box.a));
      overlay->edit->add_theme_color_override("font_color", tint);
      overlay->edit->add_theme_color_override("font_uneditable_color", tint);
      overlay->edit->add_theme_color_override("font_placeholder_color", tint);
      overlay->edit->add_theme_color_override("caret_color", tint);
      overlay->edit->set_horizontal_alignment(
          box.text_align == 1
              ? godot::HORIZONTAL_ALIGNMENT_CENTER
              : (box.text_align == 2 ? godot::HORIZONTAL_ALIGNMENT_RIGHT
                                     : godot::HORIZONTAL_ALIGNMENT_LEFT));
      overlay->edit->set_max_length(max_len.has_value() ? static_cast<int>(*max_len) : 0);
    }

    const godot::Vector2 p0 = to_canvas.xform(godot::Vector2(static_cast<float>(box.x0),
                                                             static_cast<float>(box.y0)));
    const godot::Vector2 p1 = to_canvas.xform(godot::Vector2(static_cast<float>(box.x1),
                                                             static_cast<float>(box.y1)));
    overlay->edit->set_position(p0);
    overlay->edit->set_size(p1 - p0);

    const std::u16string shown =
        from_utf8(std::string(overlay->edit->get_text().utf8().get_data()));
    const bool edited = shown != overlay->shown;
    overlay->shown = shown;
    const std::u16string source = box.input->text();
    if (shown != source) {
      if (edited) {
        box.input->set_text(shown);
      } else {
        overlay->edit->set_text(String::utf8(to_utf8(source).c_str()));
        overlay->shown = source;
      }
    }
    overlay->synced = box.input->text();

    const bool has_focus = overlay->edit->has_focus();
    if (has_focus && !box.node->focused()) {
      box.node->set_focused(true);
    } else if (!has_focus && overlay->had_focus && box.node->focused()) {
      box.node->set_focused(false);
    }
    overlay->had_focus = has_focus;
  }

  for (TextInputOverlay& stale : _text_inputs) {
    if (stale.edit == nullptr) continue;
    stale.edit->release_focus();
    remove_child(stale.edit);
    stale.edit->queue_free();
  }
  _text_inputs = std::move(next);
}

LfwGodot::TextInputOverlay* LfwGodot::last_text_input() {
  return _text_inputs.empty() ? nullptr : &_text_inputs.back();
}

void LfwGodot::destroy_text_input() {
  for (TextInputOverlay& overlay : _text_inputs) {
    if (overlay.edit == nullptr) continue;
    overlay.edit->release_focus();
    remove_child(overlay.edit);
    overlay.edit->queue_free();
  }
  _text_inputs.clear();
}

void LfwGodot::probe_text_input(int frame) {
  if (!_args.text_input_given) return;
  const int at = _args.text_input_frame;
  TextInputOverlay* const overlay = last_text_input();
  if (frame == at) {
    if (overlay == nullptr) {
      print_error("[godot] text-input: no TextInput overlay on this page");
      return;
    }
    const bool had_focus = overlay->edit->has_focus();
    overlay->edit->grab_focus();
    overlay->edit->set_text(String());
    overlay->edit->set_caret_column(0);
    overlay->edit->insert_text_at_caret(String::utf8(to_utf8(_args.text_input).c_str()));
    print_line("[godot] text-input @frame " + std::to_string(frame) + " type='" +
               to_utf8(_args.text_input) + "' focus=" + (had_focus ? "1" : "0") +
               " overlays=" + std::to_string(_text_inputs.size()));
    return;
  }
  if (frame == at + 2) {
    if (overlay != nullptr) overlay->edit->release_focus();
    return;
  }
  if (frame != at + 4) return;
  std::string players;
  const godot::Rect2 rect =
      overlay != nullptr
          ? godot::Rect2(overlay->edit->get_global_position(), overlay->edit->get_size())
          : godot::Rect2();
  if (_lfw != nullptr) {
    for (const std::pair<std::u16string, std::unique_ptr<PlayerInfo>>& kv : _lfw->players()) {
      if (!players.empty()) players += ",";
      players += to_utf8(kv.first) + ":" +
                 (kv.second != nullptr ? to_utf8(lfw::to_string(kv.second->name()))
                                      : std::string("-"));
    }
  }
  print_line("[godot] text-input done: overlay=" +
             std::string(overlay != nullptr ? "1" : "0") + " rect=" +
             (rect.size.x <= 0.0f
                  ? std::string("-")
                  : (std::to_string(static_cast<int>(rect.position.x)) + "," +
                     std::to_string(static_cast<int>(rect.position.y)) + " " +
                     std::to_string(static_cast<int>(rect.size.x)) + "x" +
                     std::to_string(static_cast<int>(rect.size.y))))
                  + " shown='" +
             to_utf8(overlay != nullptr ? overlay->shown : std::u16string()) + "' comp='" +
             to_utf8(overlay != nullptr ? overlay->synced : std::u16string()) + "' players=" +
             (players.empty() ? std::string("-") : players));
}

void LfwGodot::ui_screen_size(double& w, double& h) const {
  const lfw::host::UiScreenSize s = lfw::host::ui_screen_size(_lfw->world());
  w = s.w;
  h = s.h;
}

void LfwGodot::logical_size(double& w, double& h) const {
  if (_lfw != nullptr) {
    ui_screen_size(w, h);
    return;
  }
  w = static_cast<double>(_logical_w);
  h = static_cast<double>(_logical_h);
}

godot::Rect2 LfwGodot::contain_rect(const godot::Vector2i& window_size) const {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const double vw = static_cast<double>(window_size.x);
  const double vh = static_cast<double>(window_size.y);
  if (!(vw > 0.0) || !(vh > 0.0) || !(lw > 0.0) || !(lh > 0.0)) return godot::Rect2();
  const double aspect_l = lw / lh;
  const double aspect_v = vw / vh;
  const bool same = aspect_l == aspect_v || std::fabs(aspect_l - aspect_v) <
                                                 std::max(1e-5 * std::fabs(aspect_l), 1e-5);
  double sx = vw;
  double sy = vh;
  if (!same) {
    if (aspect_l < aspect_v) {
      sx = vh * aspect_l;
    } else {
      sy = vw / aspect_l;
    }
  }
  // `--integer-scale` 时取**整数倍**：1080p 下的自然比例是 2.4x，非整数放大让同一个
  // 像素在 2px 与 3px 之间跳，卷轴时会抖。取 floor(比例) 后多出来的部分变成黑边，
  // 画面本身是干净的整数放大（默认关，窗口/全屏都保持铺满）。
  if (pixel_integer_scaling()) {
    const double whole = std::floor(std::min(sx / lw, sy / lh));
    if (whole >= 1.0) {
      sx = lw * whole;
      sy = lh * whole;
    }
  }
  sx = std::floor(sx);
  sy = std::floor(sy);
  const double ox = sx < vw ? std::round((vw - sx) * 0.5) : 0.0;
  const double oy = sy < vh ? std::round((vh - sy) * 0.5) : 0.0;
  return godot::Rect2(static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(sx),
                      static_cast<float>(sy));
}

bool LfwGodot::pixel_integer_scaling() const {
  // 显式开关（`--integer-scale`）：像素画在非整数放大下虽然靠最近邻保住了硬边，
  // 但每个源像素落在 2px / 3px 之间来回变，卷轴时会"抖"。开了这个就把内容缩到
  // floor(比例) 的整数倍、多出来的补黑边。默认关，免得全屏时画面突然缩小一圈。
  return _args.integer_scale;
}

godot::Transform2D LfwGodot::contain_transform(const godot::Vector2i& window_size) const {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const godot::Rect2 rect = contain_rect(window_size);
  if (!(lw > 0.0) || !(lh > 0.0) || !(rect.size.x > 0.0f) || !(rect.size.y > 0.0f)) {
    return godot::Transform2D();
  }
  godot::Transform2D xform;
  xform.set_scale(godot::Vector2(static_cast<float>(rect.size.x / lw),
                                 static_cast<float>(rect.size.y / lh)));
  xform.set_origin(godot::Vector2(rect.position));
  return xform;
}

godot::Vector2i LfwGodot::window_size_px() const {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (_windowed && ds != nullptr && ds->window_get_size() != godot::Vector2i()) {
    return ds->window_get_size();
  }
  const godot::Viewport* const vp = get_viewport();
  return vp != nullptr ? godot::Vector2i(vp->get_visible_rect().size) : godot::Vector2i();
}

godot::Transform2D LfwGodot::final_transform() const {
  const godot::Viewport* const vp = get_viewport();
  return vp != nullptr ? vp->get_final_transform() : godot::Transform2D();
}

void LfwGodot::apply_game_clip(const godot::Vector2i& window_size) {
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs == nullptr || !_windowed) return;
  const godot::RID item = get_canvas_item();
  rs->canvas_item_set_custom_rect(item, true,
                                  final_transform().affine_inverse().xform(contain_rect(window_size)));
  rs->canvas_item_set_clip(item, true);
}

void LfwGodot::apply_font_oversampling(const godot::Transform2D& contain) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  const godot::Vector2 scale = contain.get_scale();
  const double over = std::max(static_cast<double>(scale.x), static_cast<double>(scale.y));
  if (!(over > 0.0)) return;
  vp->set_oversampling_override(static_cast<float>(over));
}

void LfwGodot::report_stretch(const godot::Transform2D& contain, const godot::Rect2& content) {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const godot::Vector2 scale = contain.get_scale();
  const double over = std::max(static_cast<double>(scale.x), static_cast<double>(scale.y));
  const godot::Vector2i vp_size = window_size_px();
  const godot::Transform2D final = final_transform();
  const double screen_scale = titlebar_display_scale();
  double content_scale = 1.0;
  if (godot::Viewport* const vp = get_viewport()) {
    if (const godot::Window* const win = godot::Object::cast_to<godot::Window>(vp)) {
      content_scale = static_cast<double>(win->get_content_scale_factor());
    }
  }
  char buf[448];
  std::snprintf(buf, sizeof(buf),
                "[godot] stretch: mode=disabled logical=%.0fx%.0f content=%.0fx%.0f+%.0f+%.0f "
                "scale=%.5fx%.5f oversampling=%.3f viewport=%dx%d final=%.5fx%.5f "
                "screen_scale=%.3f content_scale=%.3f",
                lw, lh, content.size.x, content.size.y, content.position.x, content.position.y,
                scale.x, scale.y, over, vp_size.x, vp_size.y, final.get_scale().x,
                final.get_scale().y, screen_scale, content_scale);
  const std::string line(buf);
  if (line == _stretch_report) return;
  _stretch_report = line;
  print_line(line);
}

inline godot::DisplayServer::CursorShape cursor_for_edge(TitleEdge edge) {
  using DS = godot::DisplayServer;
  switch (edge) {
    case TitleEdge::left:
    case TitleEdge::right:
      return DS::CURSOR_HSIZE;
    case TitleEdge::top:
    case TitleEdge::bottom:
      return DS::CURSOR_VSIZE;
    case TitleEdge::top_left:
    case TitleEdge::bottom_right:
      return DS::CURSOR_FDIAGSIZE;
    case TitleEdge::top_right:
    case TitleEdge::bottom_left:
      return DS::CURSOR_BDIAGSIZE;
    case TitleEdge::none:
    default:
      return DS::CURSOR_ARROW;
  }
}

void LfwGodot::apply_window_setup() {
  using godot::DisplayServer;
  DisplayServer* const ds = DisplayServer::get_singleton();
  if (ds == nullptr) return;
  const std::string display(ds->get_name().utf8().get_data());
  if (display == "headless") {
    print_line("[godot] window: headless display server (no window, no titlebar)");
    return;
  }
  _windowed = true;
  void* frame_hwnd = nullptr;
  const bool borderless =
      _args.titlebar == TitlebarMode::none || _args.titlebar == TitlebarMode::custom;
  if (borderless) {
    ds->window_set_flag(DisplayServer::WINDOW_FLAG_BORDERLESS, true);
  } else if (_args.titlebar == TitlebarMode::frame) {
    frame_hwnd = reinterpret_cast<void*>(
        static_cast<uintptr_t>(ds->window_get_native_handle(DisplayServer::WINDOW_HANDLE)));
    _native_frame = install_native_frame(frame_hwnd);
    const std::string evidence = native_frame_evidence(frame_hwnd);
    print_line(std::string("[godot] titlebar: frame -> native frame (captionless) ") +
               (evidence.empty() ? std::string("hwnd=-") : evidence) +
               (_native_frame ? " (WM_NCCALCSIZE=0)"
                              : " (NOT installed: no HWND or no WS_THICKFRAME)"));
  }
  const godot::Vector2i logical(_logical_w, _logical_h);
  const double scale = _args.window_scale;
  const bool explicit_size = _args.window_w > 0 && _args.window_h > 0;
  const godot::Vector2i want =
      explicit_size
          ? godot::Vector2i(_args.window_w, _args.window_h)
          : godot::Vector2i(static_cast<int>(std::lround(static_cast<double>(logical.x) * scale)),
                            static_cast<int>(std::lround(static_cast<double>(logical.y) * scale)));
  ds->window_set_flag(DisplayServer::WINDOW_FLAG_RESIZE_DISABLED, false);
  ds->window_set_min_size(logical);
  int deco_x = 0;
  int deco_y = 0;
  int in_l = 0;
  int in_t = 0;
  int in_r = 0;
  int in_b = 0;
  if (_native_frame) {
    native_frame_decoration(frame_hwnd, &deco_x, &deco_y);
    native_frame_insets(frame_hwnd, &in_l, &in_t, &in_r, &in_b);
  }
  const godot::Vector2i request =
      _native_frame ? godot::Vector2i(want.x - deco_x + in_l + in_r, want.y - deco_y + in_t + in_b)
                    : want;
  if (_native_frame) {
    char req_buf[192];
    std::snprintf(req_buf, sizeof(req_buf),
                  "[godot] window: native frame -> request %dx%d (want %dx%d, decoration %dx%d, "
                  "kept frame %dx%d)",
                  request.x, request.y, want.x, want.y, deco_x, deco_y, in_l + in_r, in_t + in_b);
    print_line(req_buf);
  }
  if (ds->window_get_size() != want) {
    ds->window_set_size(request);
    if (scale != 1.0 || explicit_size) {
      const int screen = ds->window_get_current_screen();
      const godot::Vector2i screen_pos = ds->screen_get_position(screen);
      const godot::Vector2i screen_size = ds->screen_get_size(screen);
      ds->window_set_position(screen_pos + (screen_size - want) / 2);
    }
  }
  const godot::Vector2i now = ds->window_get_size();
  const godot::Vector2i deco = ds->window_get_size_with_decorations();
  const godot::Vector2i pos = ds->window_get_position();
  const godot::Transform2D contain = contain_transform(now);
  const godot::Rect2 content = contain_rect(now);
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] window: display=%s titlebar=%s scale=%.2f borderless=%d frame=%d "
                "size=%dx%d deco=%dx%d pos=%d,%d mode=%d logical=%dx%d resizable=%d min=%dx%d "
                "content=%.0fx%.0f+%.0f+%.0f",
                display.c_str(), titlebar_name(_args.titlebar), scale,
                ds->window_get_flag(DisplayServer::WINDOW_FLAG_BORDERLESS) ? 1 : 0,
                _native_frame ? 1 : 0, now.x,
                now.y, deco.x, deco.y, pos.x, pos.y, static_cast<int>(ds->window_get_mode()),
                logical.x, logical.y,
                ds->window_get_flag(DisplayServer::WINDOW_FLAG_RESIZE_DISABLED) ? 0 : 1,
                ds->window_get_min_size().x, ds->window_get_min_size().y, content.size.x,
                content.size.y, content.position.x, content.position.y);
  print_line(buf);
  report_stretch(contain, content);
}

bool LfwGodot::titlebar_visible() const {
  if (!_windowed || !titlebar_drawn()) return false;
  return godot::DisplayServer::get_singleton() != nullptr;
}


lfw::host::TitleBarConditions LfwGodot::titlebar_conditions() const {
  lfw::host::TitleBarConditions c;
  if (_lfw == nullptr) return c;
  const lfw::World& world = _lfw->world();
  c.cheat_gim_ink = _lfw->is_cheat(lfw::cmd::kGIM_INK);
  lfw::ui::UILayers* const layers = _lfw->ui_layers();
  const lfw::ui::UINode* const ui = layers != nullptr ? layers->ui() : nullptr;
  c.has_ui = ui != nullptr;
  c.settings_page = ui != nullptr && lfw::to_string(ui->id()) == u"settings";
  const lfw::ui::UILayer* const bottom = layers != nullptr ? layers->bottom() : nullptr;
  c.bottom_pages = bottom != nullptr ? static_cast<int>(bottom->pages().size()) : 0;
  if (const lfw::Background* const bg = world.bg()) {
    c.bg_is_void = lfw::to_string(bg->id()) == u"VOID_BG";
  }
  c.first_page_init_demo = _lfw->first_page == u"init_demo";
  c.has_window = _windowed;
  c.mobile_or_toy = false;
  if (godot::DisplayServer* const ds = godot::DisplayServer::get_singleton()) {
    const godot::DisplayServer::WindowMode mode = ds->window_get_mode();
    c.fullscreen = mode == godot::DisplayServer::WINDOW_MODE_FULLSCREEN ||
                   mode == godot::DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
  }
  return c;
}

bool LfwGodot::titlebar_checked(lfw::host::TitleAction action) const {
  using lfw::host::TitleAction;
  if (_lfw == nullptr) return false;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const godot::DisplayServer::WindowMode mode =
      ds != nullptr ? ds->window_get_mode() : godot::DisplayServer::WINDOW_MODE_WINDOWED;
  switch (action) {
    case TitleAction::bgm_mute:
      return _lfw->sounds_bgm_muted();
    case TitleAction::sound_mute:
      return _lfw->sounds_sound_muted();
    case TitleAction::f1:
      return _lfw->world().paused();
    case TitleAction::f5:
      return _lfw->world().dataset.playrate() != 1.0;
    case TitleAction::fullscreen:
      return mode == godot::DisplayServer::WINDOW_MODE_FULLSCREEN ||
             mode == godot::DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
    case TitleAction::maximize:
      return native_frame() ? mode == godot::DisplayServer::WINDOW_MODE_MAXIMIZED : _maximized;
    case TitleAction::dev_ui:
    case TitleAction::f4:
    case TitleAction::settings:
    case TitleAction::minimize:
    case TitleAction::close:
    case TitleAction::none:
      break;
  }
  return false;
}

void LfwGodot::titlebar_activate(int index) {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  switch (action) {
    case TitleAction::dev_ui:
      print_line("[godot] titlebar: dev-ui (no dev panel in this host)");
      break;
    case TitleAction::f4:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF4)});
      print_line("[godot] titlebar: f4 pages=" + ui_pages_text());
      break;
    case TitleAction::f1:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF1)});
      print_line("[godot] titlebar: f1 pages=" + ui_pages_text());
      break;
    case TitleAction::f5:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF5)});
      print_line("[godot] titlebar: f5 pages=" + ui_pages_text());
      break;
    case TitleAction::settings: {
      _lfw->push_cmd({std::u16string(lfw::cmd::kF2)});
      lfw::ui::UILayers* const layers = _lfw->ui_layers();
      lfw::ui::UINode* const ui = layers != nullptr ? layers->ui() : nullptr;
      lfw::ui::UILayer* const layer = ui != nullptr ? ui->layer() : nullptr;
      if (layer != nullptr && lfw::to_string(ui->id()) == u"settings") {
        layer->pop(lfw::ui::UIPopPageOpts{});
        print_line("[godot] titlebar: settings (pop) pages=" + ui_pages_text());
      } else if (layers != nullptr) {
        auto opts = std::make_shared<lfw::Object>();
        opts->set(u"id", Value(std::u16string(u"settings")));
        layers->set_page(Value(std::move(opts)), 1.0);
        print_line("[godot] titlebar: settings (set_page) pages=" + ui_pages_text());
      }
      break;
    }
    case TitleAction::bgm_mute: {
      const bool muted = !_lfw->sounds_bgm_muted();
      _lfw->sounds_set_bgm_muted(muted);
      print_line(std::string("[godot] titlebar: bgm_muted=") + (muted ? "true" : "false"));
      break;
    }
    case TitleAction::sound_mute: {
      const bool muted = !_lfw->sounds_sound_muted();
      _lfw->sounds_set_sound_muted(muted);
      print_line(std::string("[godot] titlebar: sound_muted=") + (muted ? "true" : "false"));
      break;
    }
    case TitleAction::fullscreen: {
      if (ds == nullptr) break;
      const bool full = titlebar_checked(TitleAction::fullscreen);
      ds->window_set_mode(full ? godot::DisplayServer::WINDOW_MODE_WINDOWED
                               : godot::DisplayServer::WINDOW_MODE_FULLSCREEN);
      if (full && _maximized) {
        const godot::Rect2i usable = ds->screen_get_usable_rect(ds->window_get_current_screen());
        ds->window_set_position(usable.position);
        ds->window_set_size(usable.size);
      }
      print_line(std::string("[godot] titlebar: fullscreen=") + (full ? "false" : "true"));
      break;
    }
    case TitleAction::minimize:
      if (ds == nullptr) break;
      ds->window_set_mode(godot::DisplayServer::WINDOW_MODE_MINIMIZED);
      print_line("[godot] titlebar: minimize (" + window_state_text(ds) + ")");
      break;
    case TitleAction::maximize: {
      if (ds == nullptr) break;
      if (native_frame()) {
        const bool maxed = ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED;
        ds->window_set_mode(maxed ? godot::DisplayServer::WINDOW_MODE_WINDOWED
                                  : godot::DisplayServer::WINDOW_MODE_MAXIMIZED);
        print_line(std::string("[godot] titlebar: ") + (maxed ? "restore" : "maximize") +
                   " (native frame) (" + window_state_text(ds) + ")");
        break;
      }
      if (_maximized) {
        if (ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED) {
          ds->window_set_mode(godot::DisplayServer::WINDOW_MODE_WINDOWED);
        }
        if (_restore_size.x > 0 && _restore_size.y > 0) {
          ds->window_set_size(_restore_size);
          ds->window_set_position(_restore_pos);
        }
        _maximized = false;
        print_line("[godot] titlebar: restore (" + window_state_text(ds) + ")");
      } else {
        _restore_size = ds->window_get_size();
        _restore_pos = ds->window_get_position();
        const godot::Rect2i usable = ds->screen_get_usable_rect(ds->window_get_current_screen());
        ds->window_set_position(usable.position);
        ds->window_set_size(usable.size);
        _maximized = true;
        print_line("[godot] titlebar: maximize (" + window_state_text(ds) + ")");
      }
      break;
    }
    case TitleAction::close:
      print_line("[godot] titlebar: close");
      print_exit_summary();
      quit_with(0);
      break;
    case TitleAction::none:
      break;
  }
  queue_redraw();
}

void LfwGodot::titlebar_update_hover(const lfw::host::TitleBarLayout& layout,
                                     const lfw::host::TitleHit& hit, double bx, double by) {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleHitKind;
  const int hover = hit.kind == TitleHitKind::button ? hit.index : -1;
  int popup = -1;
  if (_bar.slider_drag >= 0) {
    popup = _bar.slider_drag;
  } else {
    for (int i = 0; i < TitleBarMetrics::count; ++i) {
      if (!layout.visible[i] || !is_volume_control(i)) continue;
      const bool open = _bar.popup == i;
      if (hover == i ||
          (open && lfw::host::titlebar_rect_contains(lfw::host::titlebar_popup_rect(layout.rect[i]),
                                                     bx, by))) {
        popup = i;
      }
    }
  }
  const bool drag_hover = hit.kind == TitleHitKind::drag;
  if (hover == _bar.hover && drag_hover == _bar.drag_hover && popup == _bar.popup) return;
  _bar.hover = hover;
  _bar.drag_hover = drag_hover;
  _bar.popup = popup;
  queue_redraw();
}

void LfwGodot::titlebar_set_volume(int index, double v) {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return;
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  if (action == TitleAction::bgm_mute) {
    _lfw->sounds_set_bgm_volume(v);
    if (v > 0.0 && _lfw->sounds_bgm_muted()) _lfw->sounds_set_bgm_muted(false);
  } else if (action == TitleAction::sound_mute) {
    _lfw->sounds_set_sound_volume(v);
    if (v > 0.0 && _lfw->sounds_sound_muted()) _lfw->sounds_set_sound_muted(false);
  } else {
    return;
  }
  queue_redraw();
}

double LfwGodot::titlebar_volume(int index) const {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return 0.0;
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  if (action == TitleAction::bgm_mute) return _lfw->sounds_bgm_volume();
  if (action == TitleAction::sound_mute) return _lfw->sounds_sound_volume();
  return 0.0;
}

double LfwGodot::titlebar_window_width(const godot::Vector2i& window_size) const {
  return static_cast<double>(window_size.x) / titlebar_display_scale();
}

double LfwGodot::titlebar_display_scale() const {
  if (_args.display_scale > 0.0) return _args.display_scale;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr || !_windowed) return 1.0;
  const double s = ds->screen_get_scale(godot::DisplayServer::SCREEN_OF_MAIN_WINDOW);
  return s > 0.0 ? s : 1.0;
}

void LfwGodot::report_titlebar_controls() {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return;
  const godot::Vector2i win_size = ds->window_get_size();
  const double bar_w = titlebar_window_width(win_size);
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, titlebar_conditions());
  const auto& list = lfw::host::titlebar_controls();
  std::string text;
  for (int i = 0; i < lfw::host::TitleBarMetrics::count; ++i) {
    if (!layout.visible[i]) continue;
    if (!text.empty()) text += " ";
    text += lfw::host::titlebar_action_name(
        list[static_cast<std::size_t>(i)].action);
  }
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] titlebar: icons=%s bar=%.0fx%.0f icon=%.1f scale=%.3f controls(%d)=%s",
                _icons_dir.empty() ? "<none>" : _icons_dir.c_str(), bar_w,
                lfw::host::TitleBarMetrics::bar_height,
                lfw::host::TitleBarMetrics::button * lfw::host::TitleBarMetrics::icon_scale,
                titlebar_display_scale(), layout.count, text.c_str());
  const std::string line(buf);
  if (line == _icon_report) return;
  _icon_report = line;
  print_line(line);
}

bool LfwGodot::titlebar_consume(const godot::Ref<godot::InputEvent>& event) {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleHit;
  using lfw::host::TitleHitKind;
  if (!titlebar_visible()) return false;
  LfwPointingEvent e;
  const PointerKind kind = to_pointing_event(event, e);
  if (kind == PointerKind::none) return false;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return false;
  const godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return false;

  const godot::Transform2D final = vp->get_final_transform();
  const godot::Vector2 win =
      final.xform(godot::Vector2(static_cast<float>(e.x), static_cast<float>(e.y)));
  const double scale = titlebar_display_scale();
  const godot::Vector2i win_size = ds->window_get_size();
  const double bx = static_cast<double>(win.x) / scale;
  const double by = static_cast<double>(win.y) / scale;
  const double win_w = static_cast<double>(win_size.x) / scale;
  const double win_h = static_cast<double>(win_size.y) / scale;
  const double bar_w = titlebar_window_width(win_size);
  const lfw::host::TitleBarConditions cond = titlebar_conditions();
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, cond);
  report_titlebar_controls();

  const bool popup_open = _bar.popup >= 0 && layout.visible[_bar.popup];
  const lfw::host::TitleBarRect popup_rect =
      popup_open ? lfw::host::titlebar_popup_rect(layout.rect[_bar.popup])
                 : lfw::host::TitleBarRect{};
  const TitleHit hit = lfw::host::titlebar_hit(bx, by, layout);
  const bool in_popup = popup_open && lfw::host::titlebar_rect_contains(popup_rect, bx, by);

  if (kind == PointerKind::move || kind == PointerKind::down) {
    titlebar_update_hover(layout, hit, bx, by);
  }

  if (kind == PointerKind::move) {
    using DS = godot::DisplayServer;
    DS::CursorShape shape =
        native_frame() ? DS::CURSOR_ARROW
                       : cursor_for_edge(titlebar_edge_at(bx, by, win_w, win_h));
    const bool on_volume = in_popup || (hit.kind == TitleHitKind::button &&
                                        is_volume_control(hit.index));
    if (shape == DS::CURSOR_ARROW && on_volume) shape = DS::CURSOR_POINTING_HAND;
    if (static_cast<int>(shape) != _cursor_shape) {
      _cursor_shape = static_cast<int>(shape);
      ds->cursor_set_shape(shape);
    }
  }

  if (!native_frame() && kind == PointerKind::down && e.button == 0.0 &&
      ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_WINDOWED &&
      !ds->window_get_flag(godot::DisplayServer::WINDOW_FLAG_RESIZE_DISABLED)) {
    const TitleEdge edge = titlebar_edge_at(bx, by, win_w, win_h);
    if (edge != TitleEdge::none) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "[godot] titlebar: resize edge=%d at=(%.0f, %.0f)",
                    static_cast<int>(edge), bx, by);
      print_line(buf);
      _bar_capture = true;
      ds->window_start_resize(to_window_edge(edge));
      return true;
    }
  }

  if (kind == PointerKind::move) {
    if (_bar.slider_drag >= 0) {
      lfw::host::TitleBarRect drag_popup;
      lfw::host::TitleBarRect drag_track;
      lfw::host::titlebar_popup_rects(layout.rect[_bar.slider_drag], drag_popup, drag_track);
      titlebar_set_volume(_bar.slider_drag, lfw::host::titlebar_slider_value(drag_track, by));
      return true;
    }
    if (hit.kind == TitleHitKind::none && !in_popup) return false;
    return true;
  }

  if (kind == PointerKind::down) {
    lfw::host::TitleBarRect down_popup;
    lfw::host::TitleBarRect down_track;
    bool down_in_popup = false;
    if (_bar.popup >= 0 && layout.visible[_bar.popup]) {
      lfw::host::titlebar_popup_rects(layout.rect[_bar.popup], down_popup, down_track);
      down_in_popup = lfw::host::titlebar_rect_contains(down_popup, bx, by);
    }
    if (e.button != 0.0) {
      return hit.kind != TitleHitKind::none || down_in_popup;
    }
    if (down_in_popup) {
      _bar_capture = true;
      titlebar_set_volume(_bar.popup, lfw::host::titlebar_slider_value(down_track, by));
      _bar.slider_drag = _bar.popup;
      return true;
    }
    if (hit.kind == TitleHitKind::button) {
      _bar_capture = true;
      titlebar_activate(hit.index);
      return true;
    }
    if (hit.kind == TitleHitKind::drag) {
      _bar_capture = true;
      const bool maxed =
          _maximized || ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED;
      bool dbl = false;
      if (const godot::InputEventMouseButton* const mb =
              godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());
          mb != nullptr) {
        dbl = mb->is_double_click();
      }
      if (dbl && (native_frame() || maxed)) {
        const auto& list = lfw::host::titlebar_controls();
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
          if (list[static_cast<std::size_t>(i)].action == lfw::host::TitleAction::maximize) {
            titlebar_activate(i);
            break;
          }
        }
        return true;
      }
      if (maxed) {
        print_line("[godot] titlebar: drag ignored (maximized)");
        return true;
      }
      if (!_bar_dragging) {
        _bar_dragging = true;
        print_line("[godot] titlebar: drag");
        ds->window_start_drag();
        _bar_dragging = false;
      }
      return true;
    }
    return false;
  }

  if (kind == PointerKind::up) {
    if (_bar.slider_drag >= 0) {
      _bar.slider_drag = -1;
      _bar_capture = false;
      return true;
    }
    if (hit.kind != TitleHitKind::none || in_popup) {
      _bar_capture = false;
      return true;
    }
  }
  if (kind == PointerKind::wheel) return hit.kind != TitleHitKind::none || in_popup;
  if (in_popup) return true;
  if (_bar_capture) {
    if (kind == PointerKind::up) _bar_capture = false;
    return true;
  }
  return false;
}

void LfwGodot::setup_icons() {
  std::vector<std::filesystem::path> probes;
  if (_args.icons_given) probes.push_back(_args.icons);
  // 打包版：优先 `<exe>/resources/icons`（它才是随包发的，别被仓库里的 src/assets 抢走）
  if (const std::filesystem::path res = packaged_resources_dir(); !res.empty())
    probes.push_back(res / "icons");
  std::error_code ec;
  probes.push_back(std::filesystem::current_path(ec));
  if (godot::OS* const os = godot::OS::get_singleton()) {
    probes.push_back(std::filesystem::path(os->get_executable_path().utf8().get_data())
                         .parent_path());
  }
  for (const std::filesystem::path& probe : probes) {
    if (probe.empty()) continue;
    std::error_code stat_ec;
    if (std::filesystem::is_regular_file(probe / "btn_1_1.png", stat_ec)) {
      _icons_dir = probe.generic_string();
    } else {
      const std::filesystem::path root = find_repo_root(probe);
      if (!root.empty()) _icons_dir = (root / "src" / "assets").generic_string();
    }
    if (!_icons_dir.empty()) break;
  }
  if (_icons_dir.empty()) {
    print_error("[godot] titlebar icons: no btn_1_1.png (给 `--icons <dir>`；条上只画命中盒)");
    return;
  }
  const std::string dir = _icons_dir;
  _icons = std::make_unique<GodotTextureCache>(
      [dir](const std::string& name, std::string& out) {
        return read_file_bytes(std::filesystem::path(dir) / name, out);
      });
  print_line("[godot] titlebar icons: " + dir);
}

void LfwGodot::apply_window_icon() {
  std::vector<std::filesystem::path> dirs;
  if (!_icons_dir.empty()) dirs.emplace_back(_icons_dir);
  if (const std::filesystem::path res = packaged_resources_dir(); !res.empty())
    dirs.push_back(res / "icons");
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr || dirs.empty()) return;
  const int64_t handle = ds->window_get_native_handle(godot::DisplayServer::WINDOW_HANDLE, 0);
  std::error_code ec;

  for (const std::filesystem::path& dir : dirs) {
    // 1) `icon.ico`：Godot 的 Image 不认 ICO，直接 Win32 `LoadImage` + `WM_SETICON`
    const std::filesystem::path ico = dir / "icon.ico";
    if (handle != 0 && std::filesystem::is_regular_file(ico, ec) &&
        lfw::host::set_window_icon(reinterpret_cast<void*>(static_cast<intptr_t>(handle)),
                                   ico.generic_string())) {
      print_line("[godot] window icon: " + ico.generic_string());
      return;
    }
    // 2) `icon.png`：走 Godot 的 set_icon
    const std::filesystem::path png = dir / "icon.png";
    std::string raw;
    if (!read_file_bytes(png, raw)) continue;
    godot::PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(raw.size()));
    std::memcpy(bytes.ptrw(), raw.data(), raw.size());
    godot::Ref<godot::Image> icon;
    icon.instantiate();
    if (icon->load_png_from_buffer(bytes) != godot::OK) {
      print_error("[godot] window icon: decode failed: " + png.generic_string());
      continue;
    }
    ds->set_icon(icon);
    print_line("[godot] window icon: " + png.generic_string());
    return;
  }
  // 两个都没有就保持 Godot 默认图标
}

void LfwGodot::draw_titlebar() {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleBarRect;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const godot::Viewport* const vp = get_viewport();
  if (ds == nullptr || vp == nullptr) return;
  const godot::Transform2D final = vp->get_final_transform();
  const double bar_w = titlebar_window_width(ds->window_get_size());
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, titlebar_conditions());
  if (!(bar_w > 0.0)) return;
  report_titlebar_controls();
  godot::Transform2D css_to_canvas;
  css_to_canvas.set_scale(godot::Vector2(static_cast<float>(titlebar_display_scale()),
                                         static_cast<float>(titlebar_display_scale())));
  draw_set_transform_matrix(final.affine_inverse() * css_to_canvas);

  const float drag_h = static_cast<float>(TitleBarMetrics::drag_height);
  if (_bar.drag_hover) {
    draw_rect(Rect2(0.0f, 0.0f, static_cast<float>(bar_w), drag_h),
              Color(0x88 / 255.0f, 0x88 / 255.0f, 0x88 / 255.0f, 0x55 / 255.0f), true);
  }

  const auto& list = lfw::host::titlebar_controls();
  for (int i = 0; i < TitleBarMetrics::count; ++i) {
    if (!layout.visible[i]) continue;
    const lfw::host::TitleBarControl& control = list[static_cast<std::size_t>(i)];
    const bool checked = titlebar_checked(control.action);
    const TitleBarRect icon = lfw::host::titlebar_icon_rect(layout.rect[i], _bar.hover == i);
    godot::Ref<godot::ImageTexture> tex;
    if (_icons != nullptr) tex = _icons->get(lfw::host::titlebar_control_image(control, checked));
    if (tex.is_null()) {
      draw_rect(Rect2(static_cast<float>(icon.x), static_cast<float>(icon.y),
                      static_cast<float>(icon.w), static_cast<float>(icon.h)),
                Color(0.85f, 0.88f, 0.95f, 0.18f), true);
      continue;
    }
    draw_texture_rect(tex,
                      Rect2(static_cast<float>(icon.x), static_cast<float>(icon.y),
                            static_cast<float>(icon.w), static_cast<float>(icon.h)),
                      false, Color(1.0f, 1.0f, 1.0f, 1.0f));
  }

  if (_bar.popup >= 0 && layout.visible[_bar.popup]) {
    TitleBarRect popup;
    TitleBarRect track;
    lfw::host::titlebar_popup_rects(layout.rect[_bar.popup], popup, track);
    const double v = titlebar_volume(_bar.popup);
    const double clamped = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    draw_rounded_rect(popup, TitleBarMetrics::popup_radius, Color(0x0F / 255.0f, 0x18 / 255.0f,
                                                                  0x30 / 255.0f, 0xCC / 255.0f));
    draw_rounded_rect(track, TitleBarMetrics::slider_w * 0.5, Color(0.0f, 0.0f, 0.0f, 0x88 / 255.0f));
    draw_rounded_rect(lfw::host::titlebar_fill_rect(track, clamped), TitleBarMetrics::slider_w * 0.5,
                      Color(0x5A / 255.0f, 0x88 / 255.0f, 0xE0 / 255.0f, 1.0f));
    const TitleBarRect knob = lfw::host::titlebar_knob_rect(track, clamped);
    draw_circle(Vector2(static_cast<float>(knob.x), static_cast<float>(knob.y)),
                static_cast<float>(knob.w * 0.5), Color(1.0f, 1.0f, 1.0f, 1.0f));
  }
  draw_set_transform_matrix(godot::Transform2D());
}

void LfwGodot::draw_clip_probe(const godot::Transform2D& contain) {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  if (!(lw > 0.0) || !(lh > 0.0)) return;
  const float w = static_cast<float>(lw);
  const float h = static_cast<float>(lh);
  const float b = 8.0f;
  const godot::Color color(1.0f, 0.0f, 1.0f, 0.85f);
  draw_set_transform_matrix(contain);
  draw_rect(Rect2(-b, -b, w + 2.0f * b, 2.0f * b), color, true);
  draw_rect(Rect2(-b, h - b, w + 2.0f * b, 2.0f * b), color, true);
  draw_rect(Rect2(-b, -b, 2.0f * b, h + 2.0f * b), color, true);
  draw_rect(Rect2(w - b, -b, 2.0f * b, h + 2.0f * b), color, true);
  draw_set_transform_matrix(godot::Transform2D());
}

void LfwGodot::draw_rounded_rect(const lfw::host::TitleBarRect& r, double radius,
                                 const godot::Color& color) {
  const double limit = r.w < r.h ? r.w : r.h;
  const float rad = static_cast<float>(radius > limit * 0.5 ? limit * 0.5 : radius);
  if (!(rad > 0.0f)) {
    draw_rect(Rect2(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w),
                    static_cast<float>(r.h)),
              color, true);
    return;
  }
  const float x = static_cast<float>(r.x);
  const float y = static_cast<float>(r.y);
  const float w = static_cast<float>(r.w);
  const float h = static_cast<float>(r.h);
  draw_rect(Rect2(x + rad, y, w - 2.0f * rad, h), color, true);
  draw_rect(Rect2(x, y + rad, w, h - 2.0f * rad), color, true);
  draw_circle(Vector2(x + rad, y + rad), rad, color);
  draw_circle(Vector2(x + w - rad, y + rad), rad, color);
  draw_circle(Vector2(x + rad, y + h - rad), rad, color);
  draw_circle(Vector2(x + w - rad, y + h - rad), rad, color);
}

void LfwGodot::push_window_click(double x, double y) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  for (int pass = 0; pass < 2; ++pass) {
    godot::Ref<godot::InputEventMouseButton> mb;
    mb.instantiate();
    mb->set_button_index(godot::MOUSE_BUTTON_LEFT);
    mb->set_pressed(pass == 0);
    mb->set_position(Vector2(static_cast<float>(x), static_cast<float>(y)));
    mb->set_global_position(mb->get_position());
    vp->push_input(mb, false);
  }
}

void LfwGodot::push_window_hover(double x, double y) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  godot::Ref<godot::InputEventMouseMotion> mm;
  mm.instantiate();
  mm->set_position(Vector2(static_cast<float>(x), static_cast<float>(y)));
  mm->set_global_position(mm->get_position());
  mm->set_relative(Vector2(0.0f, 0.0f));
  vp->push_input(mm, false);
}

LfwGodot::PointerKind LfwGodot::to_pointing_event(const godot::Ref<godot::InputEvent>& event,
                                                  LfwPointingEvent& out) const {
  double w = 794, h = 450;
  ui_screen_size(w, h);

  const godot::Ref<godot::InputEventPanGesture> pan = event;
  if (pan.is_valid()) {
    const godot::Vector2 d = pan->get_delta();
    out.delta_x = -static_cast<double>(d.x) * w;
    out.delta_y = -static_cast<double>(d.y) * h;
    out.button = 0;
    return PointerKind::wheel;
  }

  const godot::Ref<godot::InputEventMouseButton> mb = event;
  if (mb.is_valid()) {
    const godot::Vector2 pos = mb->get_position();
    out.x = static_cast<double>(pos.x);
    out.y = static_cast<double>(pos.y);
    out.delta_x = 0;
    out.delta_y = 0;
    const int64_t idx = static_cast<int64_t>(mb->get_button_index());
    if (idx == godot::MOUSE_BUTTON_WHEEL_UP || idx == godot::MOUSE_BUTTON_WHEEL_DOWN ||
        idx == godot::MOUSE_BUTTON_WHEEL_LEFT || idx == godot::MOUSE_BUTTON_WHEEL_RIGHT) {
      if (!mb->is_pressed()) return PointerKind::none;
      const double factor = std::fabs(static_cast<double>(mb->get_factor()));
      const double notch = 100.0 * (factor > 0.0 ? factor : 1.0);
      if (idx == godot::MOUSE_BUTTON_WHEEL_UP) {
        out.delta_y = -notch;
      } else if (idx == godot::MOUSE_BUTTON_WHEEL_DOWN) {
        out.delta_y = notch;
      } else if (idx == godot::MOUSE_BUTTON_WHEEL_LEFT) {
        out.delta_x = -notch;
      } else {
        out.delta_x = notch;
      }
      out.button = 0;
      return PointerKind::wheel;
    }
    out.button = idx == godot::MOUSE_BUTTON_LEFT     ? 0.0
                 : idx == godot::MOUSE_BUTTON_MIDDLE ? 1.0
                 : idx == godot::MOUSE_BUTTON_RIGHT  ? 2.0
                                                     : -1.0;
    return mb->is_pressed() ? PointerKind::down : PointerKind::up;
  }

  const godot::Ref<godot::InputEventMouseMotion> mm = event;
  if (mm.is_valid()) {
    const godot::Vector2 pos = mm->get_position();
    out.x = static_cast<double>(pos.x);
    out.y = static_cast<double>(pos.y);
    out.delta_x = 0;
    out.delta_y = 0;
    out.button = -1;
    return PointerKind::move;
  }
  return PointerKind::none;
}

std::u16string dom_key_of_godot(const godot::Ref<godot::InputEventKey>& key) {
  struct Entry {
    godot::Key code;
    const char16_t* name;
  };
  static const Entry kTable[] = {
      {godot::KEY_LEFT, u"arrowleft"},     {godot::KEY_RIGHT, u"arrowright"},
      {godot::KEY_UP, u"arrowup"},         {godot::KEY_DOWN, u"arrowdown"},
      {godot::KEY_ENTER, u"enter"},        {godot::KEY_KP_ENTER, u"enter"},
      {godot::KEY_ESCAPE, u"escape"},      {godot::KEY_TAB, u"tab"},
      {godot::KEY_BACKSPACE, u"backspace"},{godot::KEY_DELETE, u"delete"},
      {godot::KEY_INSERT, u"insert"},      {godot::KEY_HOME, u"home"},
      {godot::KEY_END, u"end"},            {godot::KEY_PAGEUP, u"pageup"},
      {godot::KEY_PAGEDOWN, u"pagedown"},  {godot::KEY_SHIFT, u"shift"},
      {godot::KEY_CTRL, u"control"},       {godot::KEY_ALT, u"alt"},
      {godot::KEY_META, u"meta"},          {godot::KEY_CAPSLOCK, u"capslock"},
      {godot::KEY_SPACE, u" "},            {godot::KEY_PERIOD, u"."},
      {godot::KEY_COMMA, u","},            {godot::KEY_SLASH, u"/"},
      {godot::KEY_SEMICOLON, u";"},        {godot::KEY_APOSTROPHE, u"'"},
      {godot::KEY_BRACKETLEFT, u"["},      {godot::KEY_BRACKETRIGHT, u"]"},
      {godot::KEY_BACKSLASH, u"\\"},       {godot::KEY_MINUS, u"-"},
      {godot::KEY_EQUAL, u"="},            {godot::KEY_QUOTELEFT, u"`"},
  };
  static const Entry kKeypad[] = {
      {godot::KEY_KP_0, u"0"}, {godot::KEY_KP_1, u"1"}, {godot::KEY_KP_2, u"2"},
      {godot::KEY_KP_3, u"3"}, {godot::KEY_KP_4, u"4"}, {godot::KEY_KP_5, u"5"},
      {godot::KEY_KP_6, u"6"}, {godot::KEY_KP_7, u"7"}, {godot::KEY_KP_8, u"8"},
      {godot::KEY_KP_9, u"9"}, {godot::KEY_KP_PERIOD, u"."}, {godot::KEY_KP_DIVIDE, u"/"},
      {godot::KEY_KP_MULTIPLY, u"*"}, {godot::KEY_KP_SUBTRACT, u"-"}, {godot::KEY_KP_ADD, u"+"},
  };
  const godot::Key code = key->get_keycode();
  for (const Entry& entry : kTable) {
    if (code == entry.code) return entry.name;
  }
  for (const Entry& entry : kKeypad) {
    if (code == entry.code) return entry.name;
  }
  if (code >= godot::KEY_F1 && code <= godot::KEY_F12) {
    return u"f" + std::u16string(1, static_cast<char16_t>(u'1' + (code - godot::KEY_F1)));
  }
  const char32_t unicode = key->get_unicode();
  if (unicode >= 32 && unicode <= 0x7e) {
    char16_t c = static_cast<char16_t>(unicode);
    if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
    return std::u16string(1, c);
  }
  if (code >= godot::KEY_A && code <= godot::KEY_Z) {
    return std::u16string(1, static_cast<char16_t>(code - godot::KEY_A + u'a'));
  }
  if (code >= godot::KEY_0 && code <= godot::KEY_9) {
    return std::u16string(1, static_cast<char16_t>(code));
  }
  return std::u16string();
}

void LfwGodot::_input(const godot::Ref<godot::InputEvent>& p_event) {
  if (!_running || _host == nullptr || _lfw == nullptr || p_event.is_null()) return;
  if (titlebar_consume(p_event)) {
    if (godot::Viewport* const vp = get_viewport()) vp->set_input_as_handled();
    return;
  }
  if (const godot::Ref<godot::InputEventKey> key_event = p_event; !key_event.is_null()) {
    const std::u16string key = dom_key_of_godot(key_event);
    if (_args.play) {
      print_line("[godot] keyraw#" + std::to_string(++_key_seq) + ": " +
                 (key_event->is_pressed() ? "down" : "up") + " dom='" + to_utf8(key) +
                 "' keycode=" + std::to_string(static_cast<long long>(key_event->get_keycode())) +
                 " phys=" +
                 std::to_string(static_cast<long long>(key_event->get_physical_keycode())) +
                 " uni=" + std::to_string(static_cast<long long>(key_event->get_unicode())) +
                 " echo=" + (key_event->is_echo() ? "1" : "0"));
    }
    if (key.empty()) return;
    if (key_event->is_pressed()) {
      const bool first = _host->pressed_keys().find(key) == _host->pressed_keys().end();
      _host->key_down(key);
      if (first) {
        print_line("[godot] key: down " + to_utf8(key));
        report_key_focus(key);
      }
    } else {
      _host->key_up(key);
      print_line("[godot] key: up " + to_utf8(key));
    }
    return;
  }
  LfwPointingEvent e;
  const PointerKind kind = to_pointing_event(p_event, e);
  if (kind == PointerKind::none) return;
  if (kind == PointerKind::down) {
    const godot::Vector2 point(static_cast<float>(e.x), static_cast<float>(e.y));
    for (TextInputOverlay& overlay : _text_inputs) {
      if (overlay.edit == nullptr || !overlay.edit->has_focus()) continue;
      const godot::Rect2 rect(overlay.edit->get_global_position(), overlay.edit->get_size());
      if (!rect.has_point(point)) overlay.edit->release_focus();
    }
  }
  const godot::Vector2 port =
      (contain_transform(window_size_px()).affine_inverse() * final_transform())
          .xform(godot::Vector2(static_cast<float>(e.x), static_cast<float>(e.y)));
  e.x = static_cast<double>(port.x);
  e.y = static_cast<double>(port.y);
  ui_scene_of_point(_lfw->world(), e.x, e.y, e.scene_x, e.scene_y);
  switch (kind) {
    case PointerKind::down:
      _host->emit_pointer_down(e);
      break;
    case PointerKind::up:
      _host->emit_pointer_up(e);
      break;
    case PointerKind::move:
      _host->emit_pointer_move(e);
      break;
    case PointerKind::wheel:
      _host->emit_wheel(e);
      break;
    case PointerKind::none:
      break;
  }
}

void LfwGodot::print_exit_summary() const {
  if (!_running || _lfw == nullptr || _host == nullptr) return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "[godot] exit: frames=%d clock=%.0fms steps=%lld pages=%s",
                _frames, _host->clock().now(), _host->steps(), ui_pages_text().c_str());
  print_line(buf);
}

void LfwGodot::_notification(int p_what) {
  if (p_what == NOTIFICATION_WM_SIZE_CHANGED) {
    queue_redraw();
    return;
  }
  if (p_what != NOTIFICATION_WM_CLOSE_REQUEST) return;
  destroy_text_input();
  print_exit_summary();
}

LfwGodot::Args::ClickAt* LfwGodot::inject_clicks(int frame) {
  if (_args.clicks.empty() || _host == nullptr || _lfw == nullptr) return nullptr;
  Args::ClickAt* first = nullptr;
  for (Args::ClickAt& click : _args.clicks) {
    if (click.frame != frame) continue;
    if (click.hover) {
      push_window_hover(click.x, click.y);
    } else if (click.window_space) {
      push_window_click(click.x, click.y);
    } else {
      _host->click_ui(*_lfw, click.x, click.y);
    }
    if (first == nullptr) first = &click;
  }
  return first;
}

const LfwGodot::Args::KeyAt* LfwGodot::inject_keys(int frame) {
  if (_args.keys.empty() || _host == nullptr || _lfw == nullptr) return nullptr;
  const Args::KeyAt* first = nullptr;
  for (const Args::KeyAt& key : _args.keys) {
    if (key.frame != frame) continue;
    if (key.down) {
      _host->key_down(key.key);
      report_key_focus(key.key);
    } else {
      _host->key_up(key.key);
    }
    if (first == nullptr) first = &key;
  }
  return first;
}

std::string LfwGodot::ui_pages_text() const {
  lfw::ui::UILayers* const layers = _lfw->ui_layers();
  if (layers == nullptr) return "<no layers>";
  std::string out;
  for (lfw::ui::UILayer* const layer : layers->all()) {
    if (layer == nullptr) continue;
    if (!out.empty()) out += " ";
    out += to_utf8(lfw::number_to_string(layer->index()));
    out += ":";
    lfw::ui::UINode* const page = layer->ui();
    out += page != nullptr ? to_utf8(lfw::to_string(page->id())) : "-";
  }
  return out.empty() ? "<empty>" : out;
}

void LfwGodot::_draw() {
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  if (renderer != nullptr) {
    const godot::Vector2i win = window_size_px();
    const godot::Transform2D contain = contain_transform(win);
    report_stretch(contain, contain_rect(win));
    apply_font_oversampling(contain);
    apply_game_clip(win);
    const GodotFrame& frame = renderer->take_frame();
    if (frame.valid) {
      _item_xform = final_transform().affine_inverse() * contain;
      draw_set_transform_matrix(_item_xform);
      static const int kPasses[3] = {-1, 0, 1};
      for (const int pass : kPasses) {
        for (const GodotDrawItem& item : frame.items) {
          if (item.pass != pass) continue;
          if (!item.text.empty()) {
            draw_text_item(item);
          } else {
            draw_item(item);
          }
        }
      }
      draw_set_transform_matrix(godot::Transform2D());
      _item_xform = godot::Transform2D();
    }
    if (_args.clip_probe) draw_clip_probe(contain);
  }
  if (titlebar_visible()) {
    godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
    if (rs != nullptr) rs->canvas_item_add_clip_ignore(get_canvas_item(), true);
    draw_titlebar();
    if (rs != nullptr) rs->canvas_item_add_clip_ignore(get_canvas_item(), false);
  }
}

void LfwGodot::draw_text_run(const godot::Ref<godot::Font>& font, const godot::Vector2& pos,
                             const std::string& line, int font_size, const godot::Color& color,
                             bool has_stroke, double stroke_w,
                             const godot::Color& stroke_color) {
  const godot::String text = godot::String::utf8(line.c_str());
  if (has_stroke && stroke_w > 0.0) {
    static const float kDirs[8][2] = {{-1.0f, 0.0f}, {1.0f, 0.0f},  {0.0f, -1.0f}, {0.0f, 1.0f},
                                      {-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
    const int n = std::max(1, static_cast<int>(std::lround(stroke_w)));
    for (int r = 1; r <= n; ++r) {
      for (int k = 0; k < 8; ++k) {
        const float d = static_cast<float>(r);
        draw_string(font, pos + godot::Vector2(kDirs[k][0] * d, kDirs[k][1] * d), text,
                    godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size, stroke_color);
      }
    }
  }
  draw_string(font, pos, text, godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size, color);
}

void LfwGodot::draw_world_text_item(const GodotDrawItem& item, const godot::Ref<godot::Font>& font,
                                    int font_size) {
  const godot::Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                           static_cast<float>(item.b), static_cast<float>(item.a));
  const bool has_stroke = item.stroke_w > 0.0 && item.stroke_a > 0.0;
  const godot::Color stroke_color(static_cast<float>(item.stroke_r),
                                  static_cast<float>(item.stroke_g),
                                  static_cast<float>(item.stroke_b),
                                  static_cast<float>(item.stroke_a));
  const double text_w = static_cast<double>(
      font->get_string_size(godot::String::utf8(item.text.c_str()),
                            godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size)
          .x);
  const double ascent = static_cast<double>(font->get_ascent(font_size));
  const double descent = static_cast<double>(font->get_descent(font_size));
  const double block_h = ascent + descent;
  double cx = item.x;
  if (item.has_clamp_x) {
    const double hw = text_w * 0.5 + item.clamp_pad;
    const double lo = item.clamp_x0 + hw;
    const double hi = item.clamp_x1 - hw;
    cx = hi >= lo ? std::min(std::max(cx, lo), hi) : (lo + hi) * 0.5;
  }
  const double center_y = item.text_below ? item.y + block_h : item.y - block_h * 0.5;
  const double left = cx - text_w * 0.5;
  const double baseline = center_y - block_h * 0.5 + ascent;
  draw_text_run(font, godot::Vector2(static_cast<float>(left), static_cast<float>(baseline)),
                item.text, font_size, color, has_stroke, item.stroke_w,
                stroke_color);
}

void LfwGodot::draw_text_item(const GodotDrawItem& item) {
  if (item.text.empty()) return;
  godot::Ref<godot::Font> font = font_for_css(item.font_css);
  if (font.is_null()) return;
  const int size = static_cast<int>(std::lround(item.font_size > 0.0 ? item.font_size : 9.0));
  const int font_size = size > 0 ? size : 1;
  if (item.world_text) {
    draw_world_text_item(item, font, font_size);
    return;
  }
  const Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                    static_cast<float>(item.b), static_cast<float>(item.a));
  const bool has_stroke = item.stroke_w > 0.0 && item.stroke_a > 0.0;
  const Color stroke_color(static_cast<float>(item.stroke_r),
                           static_cast<float>(item.stroke_g),
                           static_cast<float>(item.stroke_b),
                           static_cast<float>(item.stroke_a));
  const double pad = 2.0;
  std::vector<std::pair<std::string, double>> lines;
  double line_max = 0.0;
  for (std::size_t start = 0;;) {
    const std::size_t nl = item.text.find('\n', start);
    const std::string line = item.text.substr(
        start, nl == std::string::npos ? std::string::npos : nl - start);
    double lw = 0.0;
    if (!line.empty()) {
      lw = static_cast<double>(
          font->get_string_size(String::utf8(line.c_str()), godot::HORIZONTAL_ALIGNMENT_LEFT,
                                -1.0f, font_size)
              .x);
      line_max = std::max(line_max, lw);
    }
    lines.emplace_back(line, lw);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  const double block_w = line_max + pad * 2.0;
  const float line_h = font->get_height(font_size);
  const double block_h = static_cast<double>(lines.size()) * static_cast<double>(line_h) + pad * 2.0;
  const double content_x = item.x + item.w * 0.5 - block_w * 0.5 + pad;
  const double block_top = item.y + item.h * 0.5 - block_h * 0.5;
  double y = block_top + pad + static_cast<double>(font->get_ascent(font_size));
  for (const std::pair<std::string, double>& line : lines) {
    if (!line.first.empty()) {
      double ax = content_x;
      if (item.text_align == 1) {
        ax = content_x + std::round(line_max * 0.5) - line.second * 0.5;
      } else if (item.text_align == 2) {
        ax = content_x + std::round(line_max) - line.second;
      }
      draw_text_run(font, Vector2(static_cast<float>(ax), static_cast<float>(y)), line.first,
                    font_size, color, has_stroke, item.stroke_w, stroke_color);
    }
    y += static_cast<double>(line_h);
  }
}

namespace {

bool bytes_start_with(const std::string& b, const char* magic, std::size_t n) {
  return b.size() >= n && std::memcmp(b.data(), magic, n) == 0;
}

bool looks_like_mp3(const std::string& b) {
  if (bytes_start_with(b, "ID3", 3)) return true;
  return b.size() > 1 && static_cast<unsigned char>(b[0]) == 0xFF &&
         (static_cast<unsigned char>(b[1]) & 0xE0) == 0xE0;
}

godot::PackedByteArray to_packed(const std::string& bytes) {
  godot::PackedByteArray out;
  if (bytes.empty()) return out;
  out.resize(static_cast<int64_t>(bytes.size()));
  std::memcpy(out.ptrw(), bytes.data(), bytes.size());
  return out;
}

void set_stream_loop(const godot::Ref<godot::AudioStream>& stream, bool loop) {
  if (stream.is_null()) return;
  if (godot::AudioStreamMP3* const mp3 = godot::Object::cast_to<godot::AudioStreamMP3>(stream.ptr());
      mp3 != nullptr) {
    mp3->set_loop(loop);
    return;
  }
  if (godot::AudioStreamOggVorbis* const ogg =
          godot::Object::cast_to<godot::AudioStreamOggVorbis>(stream.ptr());
      ogg != nullptr) {
    ogg->set_loop(loop);
    return;
  }
  if (godot::AudioStreamWAV* const wav = godot::Object::cast_to<godot::AudioStreamWAV>(stream.ptr());
      wav != nullptr) {
    wav->set_loop_mode(loop ? godot::AudioStreamWAV::LOOP_FORWARD
                            : godot::AudioStreamWAV::LOOP_DISABLED);
  }
}

godot::Ref<godot::AudioStreamWAV> make_wav(const std::string& bytes) {
  const auto u16 = [&](std::size_t off) -> unsigned {
    return static_cast<unsigned>(static_cast<unsigned char>(bytes[off])) |
           (static_cast<unsigned>(static_cast<unsigned char>(bytes[off + 1])) << 8);
  };
  const auto u32 = [&](std::size_t off) -> unsigned {
    return u16(off) | (u16(off + 2) << 16);
  };
  if (bytes.size() < 44) return godot::Ref<godot::AudioStreamWAV>();
  std::size_t pos = 12;
  unsigned channels = 0, rate = 0, bits = 0, fmt_tag = 0;
  std::size_t data_off = 0, data_len = 0;
  while (pos + 8 <= bytes.size()) {
    const unsigned size = u32(pos + 4);
    const std::string id = bytes.substr(pos, 4);
    const std::size_t body = pos + 8;
    if (id == "fmt " && body + 16 <= bytes.size()) {
      fmt_tag = u16(body);
      channels = u16(body + 2);
      rate = u32(body + 4);
      bits = u16(body + 14);
    } else if (id == "data") {
      data_off = body;
      data_len = std::min<std::size_t>(size, bytes.size() - body);
    }
    pos = body + size + (size & 1u);
  }
  if (data_len == 0 || rate == 0 || channels == 0 || (bits != 8 && bits != 16)) {
    return godot::Ref<godot::AudioStreamWAV>();
  }
  godot::Ref<godot::AudioStreamWAV> wav;
  wav.instantiate();
  wav->set_data(to_packed(bytes.substr(data_off, data_len)));
  wav->set_format(fmt_tag == 3 ? godot::AudioStreamWAV::FORMAT_16_BITS
                               : (bits == 8 ? godot::AudioStreamWAV::FORMAT_8_BITS
                                            : godot::AudioStreamWAV::FORMAT_16_BITS));
  wav->set_mix_rate(static_cast<int32_t>(rate));
  wav->set_stereo(channels >= 2);
  return wav;
}

}

void LfwGodot::install_audio() {
  if (_host == nullptr) return;
  godot::AudioServer* const server = godot::AudioServer::get_singleton();
  _sfx_slots.resize(kSfxSlots);
  _sfx_pans.resize(kSfxSlots);
  _sfx_ids.assign(kSfxSlots, std::string());
  for (int i = 0; i < kSfxSlots; ++i) {
    godot::AudioStreamPlayer* const player = memnew(godot::AudioStreamPlayer);
    player->set_name(godot::String("lfw_sfx_") + godot::String::num_int64(i));
    add_child(player);
    godot::Ref<godot::AudioEffectPanner> panner;
    panner.instantiate();
    panner->set_pan(0.0f);
    if (server != nullptr) {
      const int bus = server->get_bus_count();
      server->add_bus();
      const godot::String bus_name = godot::String("lfw_sfx_") + godot::String::num_int64(i);
      server->set_bus_name(bus, bus_name);
      server->add_bus_effect(bus, panner);
      player->set_bus(bus_name);
    }
    _sfx_slots[static_cast<std::size_t>(i)] = player;
    _sfx_pans[static_cast<std::size_t>(i)] = panner;
  }
  _bgm_player = memnew(godot::AudioStreamPlayer);
  _bgm_player->set_name("lfw_bgm");
  add_child(_bgm_player);

  LfwGodotHost::AudioSink sink;
  sink.preload = [this](const std::string& path) { audio_preload(path); };
  sink.play = [this](const std::string& id, const std::string& path, double gain, double pan) {
    audio_play(id, path, gain, pan);
  };
  sink.set_gain = [this](const std::string& id, double gain, double pan) {
    audio_set_gain(id, gain, pan);
  };
  sink.stop_all = [this]() { audio_stop_all(); };
  sink.play_bgm = [this](const std::string& path, bool loop, double gain) {
    audio_play_bgm(path, loop, gain);
  };
  sink.set_bgm_gain = [this](double gain) { audio_set_bgm_gain(gain); };
  sink.stop_bgm = [this]() { audio_stop_bgm(); };
  _host->set_audio_sink(std::move(sink));

  const std::string driver = server != nullptr
                                 ? std::string(server->get_driver_name().utf8().get_data())
                                 : std::string("<none>");
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "[godot] sounds: mixer %d sfx slots + 1 bgm (per-slot bus + AudioEffectPanner) "
                "driver=%s mix=%.0fHz",
                kSfxSlots, driver.c_str(),
                server != nullptr ? static_cast<double>(server->get_mix_rate()) : 0.0);
  print_line(buf);
}

std::string LfwGodot::audio_stream_key(const std::string& path) {
  if (path.empty()) return std::string();
  const auto found = _audio_cache.find(path);
  if (found != _audio_cache.end()) {
    return found->second.is_valid() ? path : std::string();
  }
  _audio_last_path = path;
  std::string bytes;
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  if (renderer == nullptr || !renderer->read_resource(path, bytes) || bytes.empty()) {
    ++_audio_failed;
    _audio_cache[path] = godot::Ref<godot::AudioStream>();
    print_error("[godot] sounds: read failed: " + path);
    return std::string();
  }
  godot::PackedByteArray data = to_packed(bytes);
  godot::Ref<godot::AudioStream> stream;
  if (looks_like_mp3(bytes)) {
    godot::Ref<godot::AudioStreamMP3> mp3;
    mp3.instantiate();
    mp3->set_data(data);
    stream = mp3;
  } else if (bytes_start_with(bytes, "OggS", 4)) {
    stream = godot::AudioStreamOggVorbis::load_from_buffer(data);
  } else if (bytes_start_with(bytes, "RIFF", 4)) {
    stream = make_wav(bytes);
  }
  if (stream.is_null()) {
    ++_audio_failed;
    print_error("[godot] sounds: unsupported bytes: " + path);
  } else {
    ++_audio_decoded;
  }
  _audio_cache[path] = stream;
  return stream.is_valid() ? path : std::string();
}

void LfwGodot::audio_play(const std::string& id, const std::string& path, double gain, double pan) {
  if (_sfx_slots.empty()) return;
  const std::string key = audio_stream_key(path);
  if (key.empty()) return;
  const godot::Ref<godot::AudioStream>& stream = _audio_cache[key];
  int slot = -1;
  for (int i = 0; i < static_cast<int>(_sfx_slots.size()); ++i) {
    if (!_sfx_slots[static_cast<std::size_t>(i)]->is_playing()) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    slot = _sfx_next;
    _sfx_next = (_sfx_next + 1) % static_cast<int>(_sfx_slots.size());
    const std::string& stolen = _sfx_ids[static_cast<std::size_t>(slot)];
    if (!stolen.empty()) _host->on_audio_finished(stolen);
  }
  godot::AudioStreamPlayer* const player = _sfx_slots[static_cast<std::size_t>(slot)];
  if (player == nullptr) return;
  player->set_stream(stream);
  player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
  if (_sfx_pans[static_cast<std::size_t>(slot)].is_valid()) {
    _sfx_pans[static_cast<std::size_t>(slot)]->set_pan(
        static_cast<float>(std::max(-1.0, std::min(1.0, pan))));
  }
  player->play();
  _sfx_ids[static_cast<std::size_t>(slot)] = id;
}

void LfwGodot::audio_set_gain(const std::string& id, double gain, double pan) {
  for (int i = 0; i < static_cast<int>(_sfx_ids.size()); ++i) {
    if (_sfx_ids[static_cast<std::size_t>(i)] != id) continue;
    godot::AudioStreamPlayer* const player = _sfx_slots[static_cast<std::size_t>(i)];
    if (player == nullptr) continue;
    player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
    if (_sfx_pans[static_cast<std::size_t>(i)].is_valid()) {
      _sfx_pans[static_cast<std::size_t>(i)]->set_pan(
          static_cast<float>(std::max(-1.0, std::min(1.0, pan))));
    }
    return;
  }
}

void LfwGodot::audio_preload(const std::string& path) { (void)audio_stream_key(path); }

void LfwGodot::audio_play_bgm(const std::string& path, bool loop, double gain) {
  if (_bgm_player == nullptr) return;
  const std::string key = audio_stream_key(path);
  if (key.empty()) return;
  const godot::Ref<godot::AudioStream>& stream = _audio_cache[key];
  set_stream_loop(stream, loop);
  _bgm_player->set_stream(stream);
  _bgm_player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
  _bgm_player->play();
  _bgm_was_playing = true;
}

void LfwGodot::audio_set_bgm_gain(double gain) {
  if (_bgm_player == nullptr) return;
  _bgm_player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
}

void LfwGodot::audio_stop_bgm() {
  if (_bgm_player != nullptr) _bgm_player->stop();
  _bgm_was_playing = false;
}

void LfwGodot::audio_stop_all() {
  for (std::size_t i = 0; i < _sfx_slots.size(); ++i) {
    if (_sfx_slots[i] != nullptr) _sfx_slots[i]->stop();
    if (i < _sfx_ids.size()) _sfx_ids[i].clear();
  }
  audio_stop_bgm();
}

void LfwGodot::audio_poll() {
  if (_host == nullptr) return;
  for (std::size_t i = 0; i < _sfx_slots.size() && i < _sfx_ids.size(); ++i) {
    if (_sfx_ids[i].empty() || _sfx_slots[i] == nullptr) continue;
    if (_sfx_slots[i]->is_playing()) continue;
    const std::string id = _sfx_ids[i];
    _sfx_ids[i].clear();
    _host->on_audio_finished(id);
  }
  if (_bgm_player != nullptr) {
    const bool playing = _bgm_player->is_playing();
    if (_bgm_was_playing && !playing && _host->audio_bgm_random() && _lfw != nullptr) {
      _bgm_was_playing = false;
      _lfw->push_cmd(std::vector<std::u16string>{u"bgm", u"?"});
    }
    _bgm_was_playing = playing || _bgm_was_playing;
  }
  const std::string report = audio_report();
  if (report != _audio_report) {
    _audio_report = report;
    if (_args.every > 0) print_line("[godot] sounds: " + report);
  }
}

std::string LfwGodot::audio_report() const {
  if (_host == nullptr) return std::string();
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "preload=%lld decode=%lld fail=%lld play=%lld bgm=%lld active=%d bgm_playing=%d "
                "random=%d bgm_path=%s bgm_gain=%.3f last=%s",
                _host->audio_loaded(), _audio_decoded, _audio_failed, _host->audio_played(),
                _host->audio_bgm_played(), _host->audio_active(),
                _bgm_player != nullptr && _bgm_player->is_playing() ? 1 : 0,
                _host->audio_bgm_random() ? 1 : 0,
                _host->audio_bgm_path().empty() ? "-" : _host->audio_bgm_path().c_str(),
                _host->audio_bgm_gain(),
                _host->audio_last().empty() ? "-" : _host->audio_last().c_str());
  return std::string(buf);
}

void LfwGodot::draw_item(const GodotDrawItem& item) {
  if (!(item.w > 0.0) || !(item.h > 0.0)) return;
  const float x = static_cast<float>(item.x);
  const float y = static_cast<float>(item.y);
  const float w = static_cast<float>(item.w);
  const float h = static_cast<float>(item.h);
  if (item.has_model) {
    const auto found = _model_textures.find(item.tag);
    if (found == _model_textures.end() || found->second.is_null()) return;
    draw_texture_rect(found->second, Rect2(x, y, w, h), false);
    return;
  }
  const float alpha = static_cast<float>(item.a);
  const Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                    static_cast<float>(item.b), alpha);
  if (item.flat) {
    draw_rect(Rect2(x, y, w, h), color, true);
    return;
  }
  godot::Ref<godot::ImageTexture> tex;
  if (_textures != nullptr && !item.path.empty()) tex = _textures->get(item.path);
  if (tex.is_null()) {
    std::uint32_t hash = 2166136261u;
    for (const char c : item.tag) hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    const float hue = static_cast<float>(hash % 360u) / 360.0f;
    const Color tint = Color::from_hsv(hue, 0.55f, 0.85f, alpha);
    draw_rect(Rect2(x, y, w, h), tint, true);
    draw_rect(Rect2(x, y, w, h), Color(0.1f, 0.1f, 0.15f, alpha), false, 1.0f);
    return;
  }
  if (item.mix_a > 0.0 || item.gray > 0.0) {
    const godot::Ref<godot::ImageTexture> baked = _textures->get_tinted(
        item.path, item.mix_r, item.mix_g, item.mix_b, item.gray, item.mix_a);
    if (baked.is_valid()) tex = baked;
  }
  const Rect2 src(static_cast<float>(item.sx * item.tex_scale),
                  static_cast<float>(item.sy * item.tex_scale),
                  static_cast<float>(item.sw * item.tex_scale),
                  static_cast<float>(item.sh * item.tex_scale));
  const bool rotated = item.rot != 0.0;
  godot::Transform2D spin_xform;
  if (rotated) {
    godot::Transform2D rot2;
    rot2.set_rotation(static_cast<godot::real_t>(item.rot));
    if (item.flip_x) {
      godot::Transform2D local_mirror;
      local_mirror.set_scale(Vector2(-1.0f, 1.0f));
      rot2 = rot2 * local_mirror;
    }
    godot::Transform2D to_center;
    to_center.set_origin(Vector2(x + w * 0.5f, y + h * 0.5f));
    spin_xform = _item_xform * to_center * rot2;
  }
  if (item.uv_tile && !rotated && !item.flip_x) {
    const double px = item.uv_px > 0.0 ? item.uv_px : item.w;
    const double py = item.uv_py > 0.0 ? item.uv_py : item.h;
    if (px > 0.0 && py > 0.0) {
      const double ox = -std::fmod(item.uv_ox, px);
      double oy = std::fmod(item.uv_oy, py);
      if (oy > 0.0) oy -= py;
      const auto tile_pass = [&](const Color& c) {
        for (double gy = oy; gy < item.h; gy += py) {
          for (double gx = ox; gx < item.w; gx += px) {
            draw_texture_rect_region(
                tex, Rect2(static_cast<float>(item.x + gx), static_cast<float>(item.y + gy),
                           static_cast<float>(px), static_cast<float>(py)),
                src, c);
          }
        }
      };
      tile_pass(color);
      return;
    }
  }
  const auto blit = [&](const godot::Ref<godot::ImageTexture>& t, float ox, float oy, const Color& c) {
    if (rotated) {
      godot::Transform2D push;
      push.set_origin(Vector2(ox, oy));
      draw_set_transform_matrix(spin_xform * push);
      draw_texture_rect_region(t, Rect2(-w * 0.5f, -h * 0.5f, w, h), src, c);
      draw_set_transform_matrix(_item_xform);
      return;
    }
    if (item.flip_x) {
      godot::Transform2D mirror;
      mirror.set_scale(Vector2(-1.0f, 1.0f));
      mirror.set_origin(Vector2(x + ox + w, y + oy));
      draw_set_transform_matrix(_item_xform * mirror);
      draw_texture_rect_region(t, Rect2(0.0f, 0.0f, w, h), src, c);
      draw_set_transform_matrix(_item_xform);
    } else {
      draw_texture_rect_region(t, Rect2(x + ox, y + oy, w, h), src, c);
    }
  };
  if (item.outline_w > 0.0 && item.outline_a > 0.0 && !item.path.empty() && _textures != nullptr) {
    // 4 次位移剪影已预合成为一张描边层：位移量换算成纹理像素后按半径取。
    const double texels_per_px =
        item.w > 0.0 ? item.tex_scale * item.sw / item.w : 1.0;
    int radius = static_cast<int>(std::lround(item.outline_w * texels_per_px));
    if (radius < 1) radius = 1;
    // 描边取样/落盘都限定在本帧的 UV 里（纹理像素）：图集里相邻帧的像素不能参与
    const int32_t rx = static_cast<int32_t>(std::lround(item.sx * item.tex_scale));
    const int32_t ry = static_cast<int32_t>(std::lround(item.sy * item.tex_scale));
    const int32_t rw = static_cast<int32_t>(std::lround(item.sw * item.tex_scale));
    const int32_t rh = static_cast<int32_t>(std::lround(item.sh * item.tex_scale));
    const godot::Ref<godot::ImageTexture> layer =
        _textures->get_outline_layer(item.path, radius, item.outline_a, rx, ry, rw, rh);
    if (layer.is_valid()) {
      // alpha 已经烘进图里，这里只上颜色
      const Color oc(static_cast<float>(item.outline_r), static_cast<float>(item.outline_g),
                     static_cast<float>(item.outline_b), 1.0f);
      blit(layer, 0.0f, 0.0f, oc);
    }
  }
  blit(tex, 0.0f, 0.0f, color);
}

bool LfwGodot::capture_screenshot() {
  if (_args.screenshot.empty() || _captured) return false;
  _captured = true;
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs != nullptr) rs->force_draw();
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) {
    print_error("[godot] screenshot: no viewport");
    return false;
  }
  const godot::Ref<godot::ViewportTexture> vtex = vp->get_texture();
  if (vtex.is_null()) {
    print_error("[godot] screenshot: no viewport texture");
    return false;
  }
  const godot::Ref<godot::Image> img = vtex->get_image();
  if (img.is_null()) {
    print_error("[godot] screenshot: null image (headless has no renderer ⇒ drop --headless)");
    return false;
  }
  const godot::String path(_args.screenshot.generic_string().c_str());
  const godot::Error err = img->save_png(path);
  char buf[512];
  std::snprintf(buf, sizeof(buf), "[godot] screenshot: %s %dx%d err=%d frame=%d life=%.0f",
                _args.screenshot.generic_string().c_str(), img->get_width(), img->get_height(),
                static_cast<int>(err), _frames, _lfw != nullptr ? _lfw->world().lifetime() : 0.0);
  print_line(buf);
  return err == godot::OK;
}

void LfwGodot::report_frame(int frame) const {
  const lfw::World& world = _lfw->world();
  const std::string digest = frame_digest(world, frame);
  char buf[384];
  std::snprintf(buf, sizeof(buf),
                "[godot frame %6d] entities=%zu ghosts=%zu hp=%.1f digest=%s pages=%s", frame,
                world.entities.size(), world.ghosts.size(), total_hp(world), digest.c_str(),
                ui_pages_text().c_str());
  print_line(buf);
}

void LfwGodot::finish() {
  _running = false;
  const lfw::World& world = _lfw->world();
  const std::string digest = frame_digest(world, _frames - 1);
  GodotWorldRenderer* const renderer = _host->world_renderer();
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] done: frames=%d ups=%.0f digest=%s warns=%d errors=%d ignored_images=%lld "
                "renderer(+-)=%lld/%lld clock=%.1fms residual=%.1fms steps=%lld",
                _frames, 1000.0 / _host->step_ms(), digest.c_str(), _host->warn_count(),
                _host->error_count(), _host->ignored_images(),
                renderer != nullptr ? renderer->added() : 0,
                renderer != nullptr ? renderer->removed() : 0, _host->clock().now(),
                _host->residual_ms(), _host->steps());
  print_line(buf);
  std::snprintf(buf, sizeof(buf), "[godot] UI render calls: %lld (unhandled: %lld)",
                world.ui_render_calls(), world.ui_render_unhandled());
  print_line(buf);
  print_line("[godot] sounds: " + audio_report());
  if (renderer != nullptr) {
    const GodotFrame& f = renderer->take_frame();
    std::snprintf(
        buf, sizeof(buf),
        "[godot] render: bg=%s cam=%.1f,%.1f scale=%.2f,%.2f df=%.3f items=%zu bg_layers=%d "
        "ents=%d hidden=%d no_pic=%d no_file=%d unnamed_tex=%d outline=%d[%s] rot=%d@%.4f "
        "model=%d[%s] ol=%lld textures=%lld ok/%lld fail uv=%d[%s]",
        f.bg_id.c_str(), f.cam_x, f.cam_y, f.scale_x, f.scale_y, f.dfactor, f.items.size(),
        f.bg_layers, f.drawn_entities, f.hidden_entities, f.no_pic_entities, f.no_file_entities,
        f.unnamed_textures, f.outline_items, f.outline_probe.c_str(), f.rotated_items,
        f.rotated_probe, f.model_items, f.model_probe.c_str(),
        _textures != nullptr ? _textures->outline_layers() : 0,
        _textures != nullptr ? _textures->loaded() : 0,
        _textures != nullptr ? _textures->failed() : 0, f.uv_items, f.uv_probe.c_str());
    print_line(buf);
    {
      const lfw::GenFieldStats& gen = lfw::gen_field_stats();
      std::snprintf(buf, sizeof(buf), "[godot] gen_field: calls=%lld resolved=%lld closures=%lld",
                    gen.calls, gen.resolved, gen.closures);
      print_line(buf);
    }
    if (_textures != nullptr && !_textures->last_error().empty()) {
      print_line("[godot] last texture error: " + _textures->last_error());
    }
    if (_models != nullptr && (!_models->last_error().empty() || _models->loaded() > 0)) {
      char mline[512];
      std::snprintf(mline, sizeof(mline),
                    "[godot] models: loaded=%lld failed=%lld live=%lld %s%s",
                    _models->loaded(), _models->failed(), _models->live(),
                    _models->report().c_str(), _models->last_error().c_str());
      print_line(mline);
    }
    const GodotUIStats& ui = renderer->ui_stats();
    std::snprintf(buf, sizeof(buf),
                  "[godot] ui: layers=%lld nodes=%lld drawn=%lld items=%lld texts=%lld hidden=%lld "
                  "empty=%lld clipped=%lld missing_img=%lld total_items=%zu",
                  ui.layers, ui.nodes, ui.drawn, ui.items, ui.texts, ui.hidden, ui.empty,
                  ui.clipped, ui.missing_img, f.items.size());
    print_line(buf);
  }
  if (_args.ui_dump && renderer != nullptr) {
    for (const std::string& line : renderer->ui_dump_lines()) print_line(line);
  }
  quit_with(0);
}

void LfwGodot::quit_with(int code) const {
  godot::SceneTree* const tree = get_tree();
  if (tree != nullptr) tree->quit(code);
}

void LfwGodot::_bind_methods() {}

}
