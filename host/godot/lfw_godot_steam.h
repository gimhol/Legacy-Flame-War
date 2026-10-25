#pragma once

#include <string>

namespace lfw::host {

/** `--steam` 的三档：auto = 编进来了就试，失败就只是关掉 */
enum class SteamMode { kAuto = 0, kOff = 1, kOn = 2 };

/**
 * Steamworks 运行时（SDK 在 `host/godot/third/steam`，被 .gitignore 忽略）。
 *
 * 三层开关，任何一层关掉都只是「不接」，游戏照跑：
 *   1. 编译期 `LFW_GODOT_STEAM`（CMake option，缺 SDK 时自动 OFF）
 *   2. 运行时 `--steam=0|1|auto`
 *   3. 环境：`steam_api64.dll` 不在、Steam 客户端没开、没有 appid ⇒ `SteamAPI_Init()`
 *      自己失败，这里只记一行原因。
 */
class SteamRuntime {
 public:
  SteamRuntime() = default;
  ~SteamRuntime();

  SteamRuntime(const SteamRuntime&) = delete;
  SteamRuntime& operator=(const SteamRuntime&) = delete;

  static bool compiled_in();

  bool init(SteamMode mode);
  void poll();
  void shutdown();

  bool enabled() const { return _enabled; }
  const std::string& error() const { return _error; }
  /** Steam 客户端允许在这个进程里弹 Overlay 吗（`ISteamUtils::IsOverlayEnabled`） */
  bool overlay_enabled() const;
  /** 主动弹 Overlay（`friends` / `community` / `players` / `settings` / `achievements` …） */
  void open_overlay(const char* dialog = "friends");
  /** 一行事实：`on appid=480 user="gim" id=... overlay=1` / `off (built without Steamworks)` */
  std::string status() const;

 private:
  bool _enabled = false;
  std::string _error;
  std::string _user_name;
  unsigned _app_id = 0;
  unsigned long long _steam_id = 0;
};

}
