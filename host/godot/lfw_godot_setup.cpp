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


}


void LfwGodot::_ready() {
#if LFW_PERF
  calibrate_tsc();
  lfw::perf::set_now(&steady_now_ms);
#endif
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
  _console.create(this);
  _console.set_font_provider([this](const std::string& css) { return font_for_css(css); });
  _console.set_exec([this](const std::string& line) {
    print_line("[godot] console: " + line);
    if (_lfw != nullptr) _lfw->push_cmd({from_utf8(line)});
  });
  _console.set_open_hook([this] {
    release_game_keys();
    _text_inputs.release_focus();
    print_line("[godot] console: open (Enter 执行, Ctrl+` / Esc 关闭)");
  });
  _console.set_close_hook([this] {
    release_game_keys();
    print_line("[godot] console: closed");
  });
  _console.edit()->connect("text_submitted",
                           callable_mp(this, &LfwGodot::_on_console_submitted));
  _console.scroll_bar()->connect("value_changed",
                                 callable_mp(this, &LfwGodot::_on_console_scroll));
  if (_args.console) _console.open();
  run_console_scripts(-1);
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

void LfwGodot::_on_console_submitted(const godot::String& text) {
  _console.submit(text.utf8().get_data());
}

void LfwGodot::_on_console_scroll(double value) { _console.on_scrollbar_value(value); }

void LfwGodot::release_game_keys() {
  if (_host == nullptr) return;
  std::vector<std::u16string> keys;
  keys.reserve(_host->pressed_keys().size());
  for (const std::pair<const std::u16string, int>& kv : _host->pressed_keys()) {
    keys.push_back(kv.first);
  }
  for (const std::u16string& key : keys) _host->key_up(key);
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
  _host->set_console_sink([this](int kind, const std::string& text) {
    _console.append(kind, text);
    if (_args.console_echo) print_line("[console] " + text);
  });
  install_text_metrics();
  install_audio();
  lfw::set_clock(&_host->clock());
  lfw::set_timeout(&_host->timeout());
  _host->clock().set(_args.time);

  std::vector<lfw::LFW::ZipItem> items;
  // 远端数据包：HTTP + 落盘缓存都靠宿主的几个钩子。
  // `--no-remote-cache` = 照样联网，但落到临时目录、退出即删（不污染 user://）。
  {
    bool persist = !_args.no_remote_cache;
    std::filesystem::path cache_dir = persist ? user_remote_cache_dir() : std::filesystem::path();
    if (!persist) {
      std::error_code ec;
      const auto stamp = static_cast<unsigned long long>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count());
      cache_dir = std::filesystem::temp_directory_path(ec) / "lfw-remote-cache" /
                  std::filesystem::path(std::to_string(stamp));
      if (ec) cache_dir.clear();
    }
    if (!cache_dir.empty()) _host->enable_remote_cache(cache_dir, persist);
  }
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
  // 远端包排在本地包之后：本地存在时优先，远端只做补充/更新
  for (const std::u16string& url : _args.remote_zips) {
    print_line("[godot] remote package: " + to_utf8(url));
    items.push_back(lfw::LFW::ZipItem{url, nullptr});
  }
  for (IZip* const zip : mod_packs) items.push_back(lfw::LFW::ZipItem{u"", zip});

  const std::string version_text = resolve_version_name();
  LFW::VERSION_NAME() = from_utf8(version_text);
  print_line("[godot] version name: " + version_text);
  if (!_args.version_check_url.empty()) {
    _host->check_remote_version(_args.version_check_url);
  }

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
    } else if (msg == u"stats_visible_set:1" || msg == u"stats_visible_set:0") {
      set_stats_visible(msg == u"stats_visible_set:1");
    } else if (msg == u"stats_visible_get") {
      std::u16string reply = u"stats_visible:";
      reply += _stats_visible ? u"1" : u"0";
      _lfw->broadcast(Value(reply));
    }
  });
  set_stats_visible(_args.stats);

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

  if (_args.sync_render >= 0) {
    // 渲染频率模式：设置页那一行改的就是这个数据集字段
    _lfw->world().dataset.set(u"sync_render", Value(static_cast<double>(_args.sync_render)));
    print_line("[godot] sync_render: " + std::to_string(_args.sync_render));
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

  if (_args.no_ui) {
    if (lfw::ui::UILayers* const layers = _lfw->ui_layers()) {
      for (lfw::ui::UILayer* const layer : layers->all()) {
        if (layer == nullptr) continue;
        if (lfw::ui::UINode* const page = layer->ui()) page->set_visible(false);
      }
      print_line("[godot] ui: hidden (--no-ui)");
    }
  }

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
  _steam.init(_args.steam);
  print_line("[godot] steam: " + _steam.status());
  if (_args.steam_overlay && _steam.enabled()) _steam_overlay_pending = true;
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
  std::string fam = spec.family;
  for (char& c : fam) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (fam == "monospace" || fam == "mono") return _fonts.mono(spec.weight);
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

// 音频实现在 `lfw_godot_audio.cpp`：这里只负责把「怎么取字节」「BGM 播完怎么办」接上。
void LfwGodot::install_audio() {
  if (_host == nullptr) return;
  _audio.set_reader([this](const std::string& path, std::string& bytes) {
    GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
    return renderer != nullptr && renderer->read_resource(path, bytes);
  });
  _audio.set_finished_hook([this]() {
    if (_lfw != nullptr) _lfw->push_cmd(std::vector<std::u16string>{u"bgm", u"?"});
  });
  _audio.install(*this, *_host);
}

}
