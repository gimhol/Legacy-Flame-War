#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "host/godot/lfw_godot_node.h"
#include "host/godot/lfw_godot_titlebar.h"
#include "lfw/utils/utf8.h"

namespace lfw::host {

#if LFW_PERF
extern double g_tsc_per_ms;
extern long long g_perf_window_start;
double steady_now_ms();
void calibrate_tsc();
#endif

namespace godot_node {

/**
 * `lfw_godot_*.cpp` 之间共用的小工具（`LfwGodot` 的成员函数按职责拆到几个 TU，
 * 这些原来在 `lfw_godot_node.cpp` 匿名命名空间里的东西挪到这里，避免各 TU 各抄一份）。
 */

inline std::u16string from_utf8(const std::string& s) {
  return decode_utf8(std::vector<uint8_t>(s.begin(), s.end()));
}

inline std::string to_utf8(const std::u16string& s) {
  const std::vector<uint8_t> buf = encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

inline void print_line(const std::string& text) {
  godot::UtilityFunctions::print(godot::String::utf8(text.c_str()));
}

inline void print_error(const std::string& text) {
  godot::UtilityFunctions::printerr(godot::String::utf8(text.c_str()));
}

inline double wall_now_ms() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

inline bool is_volume_control(int index) {
  const auto& list = titlebar_controls();
  if (index < 0 || index >= static_cast<int>(list.size())) return false;
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  return action == TitleAction::bgm_mute || action == TitleAction::sound_mute;
}

inline bool read_file_bytes(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !out.empty();
}

inline std::filesystem::path find_repo_root(std::filesystem::path start) {
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

inline std::filesystem::path exe_dir() {
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
inline std::filesystem::path packaged_resources_dir() {
  const std::filesystem::path dir = exe_dir() / "resources";
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return {};
  return dir;
}

inline std::filesystem::path user_data_dir() {
  godot::OS* const os = godot::OS::get_singleton();
  if (os == nullptr) return {};
  const godot::String dir = os->get_user_data_dir();
  if (dir.is_empty()) return {};
  return std::filesystem::path(dir.utf8().get_data());
}

/** 用户数据目录（Godot 的 `user://`）下的 `mods/` */
inline std::filesystem::path user_mods_dir() { return user_data_dir() / "mods"; }

/** 用户数据目录（Godot 的 `user://`）下的 `cache/data`：远端数据包的落盘缓存 */
inline std::filesystem::path user_remote_cache_dir() { return user_data_dir() / "cache" / "data"; }

/**
 * 「自定义游戏」选好的包暂存到这里：重启后优先加载，加载完就删 ——
 * 只对本次会话生效（下次正常启动还是默认数据）。
 */
inline std::filesystem::path pending_custom_game_file() {
  return user_data_dir() / "custom-game" / "pending.zip";
}

inline std::string cwd_string() {
  std::error_code ec;
  const std::filesystem::path cwd = std::filesystem::current_path(ec);
  return ec ? std::string("<unknown>") : cwd.generic_string();
}

inline std::string lower_ascii(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

inline const char* titlebar_name(LfwGodot::TitlebarMode mode) {
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

inline godot::DisplayServer::WindowResizeEdge to_window_edge(TitleEdge edge) {
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

inline std::string window_state_text(const godot::DisplayServer* ds) {
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

}
