# `third/`

仓库外输入（不入库）：`third/.gitignore` 忽略的是**整个** `third/steam`。

## `third/steam` —— Steamworks SDK

把 Steamworks 官方包（`steamworks_sdk_<ver>.zip`，解出来能看到 `public/`、
`redistributable_bin/`、`steamworksexample/`、`tools/`、`Readme.txt`）**整个**解到
`native/host/godot/third/steam/`，最终要有这些路径：

```
native/host/godot/third/steam/public/steam/steam_api.h
native/host/godot/third/steam/redistributable_bin/win64/steam_api64.lib
native/host/godot/third/steam/redistributable_bin/win64/steam_api64.dll
```

Godot 宿主的接法（[`lfw_godot_steam.h`](../lfw_godot_steam.h) /
[`lfw_godot_steam.cpp`](../lfw_godot_steam.cpp)）是**三层开关，任何一层关掉都只是「不接」，游戏照跑**：

| 层 | 开关 | 缺省 |
|---|---|---|
| 编译期 | CMake `-DLFW_GODOT_STEAM=ON/OFF`，或环境变量 `LFW_GODOT_STEAM=0/1`（`npm run native godot` 转成 `-D`） | SDK 目录在 ⇒ `ON`；不在 ⇒ `OFF` |
| 运行时 | `--steam=0` / `--steam=1` / `--steam=auto` | `auto`：编进来了就试，失败只记一行原因 |
| 环境 | `steam_api64.dll` 在不在、Steam 客户端登录没、appid 拿不拿得到 | 由 `SteamAPI_Init()` 自己判 |

- 编译期 `ON` 但 SDK 不在 ⇒ CMake 直接 `FATAL_ERROR`（不会悄悄少接）；`OFF` 时
  `--steam` 恒报 `off (built without Steamworks (LFW_GODOT_STEAM=OFF))`。
- 只在启动/退出各打一行事实：`[godot] steam: on appid=… user="…" id=…` /
  `[godot] steam: off (SteamAPI_Init() failed: steam_api64.dll / Steam client / appid)`。
- 目前接的是最小闭环：`SteamAPI_Init()` → 每帧 `SteamAPI_RunCallbacks()` → 退出
  `SteamAPI_Shutdown()`。成就 / 云存档 / 工作坊都还没接。
- **Overlay（Shift+Tab）**：Steam 客户端自己会拦 Shift+Tab；它没拦到时（见下）宿主在
  `_input` 里自己调 `ISteamFriends::ActivateGameOverlay("friends")`。状态行的 `overlay=0/1`
  就是 `ISteamUtils::IsOverlayEnabled()` 的实时值 —— 这个 0 说明**客户端没注入**，不是宿主没调。

### ⚠ Overlay 与渲染后端（实测）

同一份代码、同一个 appid、同一台机器：

| 渲染后端 | `IsOverlayEnabled()` |
|---|---|
| `forward_plus`（Vulkan，Godot 缺省） | 一直是 `0`（跑满 300 帧也不变）→ Shift+Tab 无反应 |
| `gl_compatibility`（OpenGL 3.3） | 开局几十帧内变 `1` → Shift+Tab / `ActivateGameOverlay` 都正常 |

所以 `project.godot` 的 `rendering/renderer/rendering_method` 定成了 `gl_compatibility`
（纯 2D 像素游戏本来也更适合这一档）。想用 Vulkan 覆盖：`lfw.exe --rendering-driver vulkan`
（引擎参数写在 `--` 之前，但那样 Overlay 大概又没了）。

排查顺序：`[godot] steam:` 那行 `overlay=?` → 0 时先看 Steam 设置「游戏内 → 启用 Steam 界面」
有没有开，再看渲染后端。

### 跑起来需要什么

- **`npm run godot`（= `godot-run`）已经替你把 DLL 找到了**：只要 `third/steam/.../win64/steam_api64.dll`
  在，它就把这个目录塞进子进程的 `PATH`；`lfw_godot.dll` 加载时按 PATH 找得到，Steam 就能真开。
  唯一还要你给的是 appid：把 `host/godot/project/steam_appid.txt`（内容 = appid 数字）放好即可
  —— `--path` 会把进程 cwd 切到工程目录，Steam 就是在那里找这个文件。缺了它只会打一行
  `steam: off (SteamAPI_Init: No appID found…)`，游戏照跑。
- **DLL 位置**：打包版（`npm run godot:pack`）由脚本把 SDK 里那份 `steam_api64.dll` 放到
  `lfw.exe` 旁边（Windows 默认 DLL 搜索先看 exe 目录）；用官方 Godot 编辑器跑时则靠上面的 PATH。
- **发行版不要带 `steam_appid.txt`**：从 Steam 启动时 appid 由客户端给。
