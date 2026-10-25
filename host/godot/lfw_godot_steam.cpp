#include "host/godot/lfw_godot_steam.h"

#include <cstdio>
#include <string>

#if LFW_GODOT_STEAM
#include <steam/steam_api.h>

#include <windows.h>

namespace {

/** 延迟加载的前提：先自己探一下 DLL 在不在，不在就一次都不碰 Steam 符号 */
bool steam_dll_present() {
  static const int cached = [] {
    const HMODULE h = ::LoadLibraryA("steam_api64.dll");
    return h != nullptr ? 1 : 0;
  }();
  return cached == 1;
}

}

#endif

namespace lfw {
namespace host {

SteamRuntime::~SteamRuntime() { shutdown(); }

bool SteamRuntime::compiled_in() {
#if LFW_GODOT_STEAM
  return true;
#else
  return false;
#endif
}

bool SteamRuntime::init(SteamMode mode) {
  if (_enabled) return true;
  _error.clear();
  if (mode == SteamMode::kOff) {
    _error = "--steam=0";
    return false;
  }
#if !LFW_GODOT_STEAM
  _error = "built without Steamworks (LFW_GODOT_STEAM=OFF)";
  return false;
#else
  if (!steam_dll_present()) {
    _error = "steam_api64.dll not found (put it next to the host exe)";
    return false;
  }
  SteamErrMsg msg{};
  const ESteamAPIInitResult rc = SteamAPI_InitEx(&msg);
  if (rc != k_ESteamAPIInitResult_OK) {
    _error = msg[0] != '\0' ? std::string("SteamAPI_Init: ") + msg
                            : "SteamAPI_Init failed (rc=" + std::to_string(static_cast<int>(rc)) + ")";
    return false;
  }
  _enabled = true;
  if (ISteamUser* const user = SteamUser(); user != nullptr) {
    _steam_id = user->GetSteamID().ConvertToUint64();
  }
  if (ISteamFriends* const friends = SteamFriends(); friends != nullptr) {
    const char* const name = friends->GetPersonaName();
    if (name != nullptr) _user_name = name;
  }
  if (ISteamUtils* const utils = SteamUtils(); utils != nullptr) {
    _app_id = utils->GetAppID();
  }
  return true;
#endif
}

void SteamRuntime::poll() {
#if LFW_GODOT_STEAM
  if (_enabled) SteamAPI_RunCallbacks();
#endif
}

void SteamRuntime::shutdown() {
#if LFW_GODOT_STEAM
  if (!_enabled) return;
  _enabled = false;
  SteamAPI_Shutdown();
#endif
}

std::string SteamRuntime::status() const {
  char buf[384];
  if (_enabled) {
    std::snprintf(buf, sizeof(buf), "on appid=%u user=\"%s\" id=%llu", _app_id,
                  _user_name.c_str(), _steam_id);
    return std::string(buf);
  }
  std::snprintf(buf, sizeof(buf), "off (%s)", _error.empty() ? "not initialized" : _error.c_str());
  return std::string(buf);
}

}
}
