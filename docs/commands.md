# 命令参考

打包与运行时的专题文档： [打包](packaging.md) · [数据包与压缩](data-packages.md) ·
[模组](mods.md) · [游戏内控制台](console.md)。

## 前置

| 需要 | 说明 |
|---|---|
| CMake ≥ 3.20 + Ninja | `build/msvc-x64` 用 Ninja 生成器 |
| C++20 编译器 | Windows 上默认找 MSVC 2022；其它平台用 `CMakePresets.json` 的预设 |
| Node.js | `tools/` 下的脚本都是 ESM |

只编 Godot 宿主时另需：

| 需要 | 说明 |
|---|---|
| Godot 4.7+（本机 4.7.2） | 跑 `godot-run`、dump API；用 `GODOT_BIN` 指向 console 版可拿到 stdout |
| godot-cpp 检出 | 环境变量 `GODOT_CPP_DIR` 指向它；上游最新只到 `4.5` 分支，用 4.5 检出即可 |
| `build/godot-api/extension_api.json` | 由本机 Godot 生成，见下 |

```powershell
# 1) godot-cpp 放在仓库外（换路径的话同步改 GODOT_CPP_DIR）
git clone --branch 4.5 --depth 1 https://github.com/godotengine/godot-cpp "$env:USERPROFILE\godot-cpp-4.5"
[Environment]::SetEnvironmentVariable('GODOT_CPP_DIR', "$env:USERPROFILE\godot-cpp-4.5", 'User')
[Environment]::SetEnvironmentVariable('GODOT_BIN', 'C:\path\to\Godot_v4.7.2-stable_win64_console.exe', 'User')

# 2) 用本机 Godot 自己 dump API（4.7.x 的 ABI 一致；必须与 .gdextension 的 compatibility_minimum 对齐）
New-Item -ItemType Directory -Force build/godot-api | Out-Null
Push-Location build/godot-api
& $env:GODOT_BIN --headless --dump-extension-api --dump-gdextension-interface
Pop-Location
```

`GODOT_CPP_DIR` / `GODOT_BIN` 只在这两个脚本里用：`tools/native.mjs` 与 `tools/package-godot.mjs`
先看进程环境，找不到再读用户级 / 系统级注册表 ⇒ **不用重开终端或 VS Code**，`SetEnvironmentVariable`
之后直接跑就行（脚本 spawn 出去的 CMake/Godot 拿到的是脚本解析好的路径）。上面第 2 步那种在终端里
手敲的命令不经过脚本，还是要先把值取进当前进程：
`$env:GODOT_BIN = [Environment]::GetEnvironmentVariable('GODOT_BIN', 'User')`。

改了 `lf2s/origin/prel_data/**`（例如新增 `directors/*.director.xml`）后，要回仓库根目录跑
`npm run build:prel`，再把新的 `public/prel.zip` 同步到 `dist/prel.zip`（或重新解包到
`build/host-data/prel`）。否则游戏内「测试用例页」读到的还是旧数据，只会显示 `None`。

## npm scripts

| 命令 | 等价于 |
|---|---|
| `npm run native <sub>` | `node tools/native.mjs <sub>` |
| `npm run build` / `lint` / `test` / `coverage` | 对应的 `native.mjs` 子命令 |
| `npm run godot:build` | `node tools/native.mjs godot` |
| `npm run godot` | `node tools/native.mjs godot-run` |
| `npm run godot:headless` | `node tools/native.mjs godot-run --headless` |
| `npm run godot:pack` | `node tools/package-godot.mjs`（免安装目录） |
| `npm run godot:pack:zip` | `node tools/package-godot.mjs --zip`（目录 + zip） |
| `npm run intellisense` | 重算合并后的 `build/compile_commands.json` |

## tools/native.mjs

```
node tools/native.mjs <configure|build|lint|coverage|test|godot|godot-run|intellisense|all> [...]
```

| 子命令 | 作用 |
|---|---|
| `configure` | 配置 `build/msvc-x64`，并写合并的 IntelliSense 编译库 |
| `build` | 编核心 + headless 宿主 → `build/msvc-x64/bin/lfw_run.exe` |
| `lint` | `lfw/` 的硬规则检查（禁用的头文件 / 调用 / 语言设施） |
| `coverage` | defines 枚举 / 字段表 / schema 的覆盖对照 |
| `godot` | 编 GDExtension → `host/godot/project/bin/lfw_godot.dll` |
| `godot-run` | 跑 Godot 工程；`--` 之后的参数原样透传给宿主 |
| `intellisense` | 只重算 `build/compile_commands.json` |
| `all` | `build` → `lint` |

`godot-run` 自己找 Godot：环境变量 `GODOT_BIN` → `PATH` 上的 `godot4` / `godot`
→ 桌面 / 用户主目录（含 `Godot_v*` 子目录）下的 `Godot_v*-stable_win64[_console].exe`。
同版本优先 `_console` 变体（Windows 上只有它会打 stdout）。

## 打包（`tools/package-godot.mjs`）

```
node tools/package-godot.mjs [--zip] [--unpack]
```

无路径参数，输入全部由脚本推导；产物是 `release/Legacy Flame War_godot_<版本>/`（`--zip` 时
再加同名 `.zip`），双击包内 `lfw.exe` 即玩。

产物布局、零参数探测规则、去 Godot 标注（启动画面 / 标题 / 图标）见
[packaging.md](packaging.md)；数据包与压缩方法见 [data-packages.md](data-packages.md)；
模组目录与自定义游戏包见 [mods.md](mods.md)。

## headless 宿主（`lfw_run`）

```
lfw_run --root <dir> --zip <dir> [--zip <dir> ...] [--stage <id>] [--frames N]
        [--players N] [--fighter <id>] [--time ms] [--every N] [--dev]
        [--click <x>,<y>[@<frame>]] [--click-frame N]
        [--key <name>[@<frame>]] [--key-down <name>[@<frame>]] [--key-up <name>[@<frame>]]
        [--key-frame N] [--key-tap N]
        [--zip-diff <zip>,<dir>]
```

| 参数 | 说明 |
|---|---|
| `--root <dir>` | 资源根目录（含 `builtin_data/`），可重复 |
| `--zip <dir>` | 解包后的数据包目录或 `.zip` 文件，可重复，按给定顺序 load |
| `--fighter <id>` | 指定出场角色，可重复 |
| `--frames N` | 推进 N 个逻辑帧后收工并打摘要 |
| `--every N` | 每 N 帧打一行摘要 |
| `--click <x>,<y>[@frame]` | 在指定帧注入一次点击（逻辑像素） |
| `--key <name>[@frame]` | 在指定帧按一下（`space` / `esc` / `up` / `j`…）；`--key-down` / `--key-up` 只按一半 |
| `--zip-diff <zip>,<dir>` | 只逐条比对真 zip 与解包目录，有差异则退出码 1 |

## Godot 宿主（`godot-run` 透传的参数）

```bash
node tools/native.mjs godot-run [--headless] -- --zip <dir> [--zip <dir> ...] ...
```

不给 `--zip` 时：`build/host-data/{prel,data}` 存在就用解包目录，否则直接读
`dist/{prel,data}.zip`（`--zip` 的顺序就是加载顺序）。

| 参数 | 说明 |
|---|---|
| `--headless` | 无窗口（只打摘要；截图会得到空白） |
| `--root <dir>` | 资源根目录，可重复；缺省 `.` |
| `--zip <dir>` | 解包后的数据包目录或 `.zip` 文件，可重复（zip 只读索引 + 用到的条目，整包不进内存） |
| `--mods <dir>` | 额外模组目录（放 `*.zip`），可重复；比默认目录后加载（优先级更高） |
| `--drop <zip>` | 诊断：启动后按「拖拽」路径把一个 zip 当模组热加载（和窗口里拖入同一条信号链），可重复 |
| `--custom-game <zip>` | 诊断：启动后走「自定义游戏」路径（暂存 + 自动重启）加载一个 full.zip |
| `--no-mods` | 不自动加载模组（默认目录与 `--mods` 都跳过） |
| `--console` | 启动时就把控制台（Ctrl+`）打开，见 [console.md](console.md) |
| `--console-exec <行>` | 启动后按控制台输入执行这条命令，可重复（诊断 / 验收） |
| `--lang <code>` | UI 语言码（`zh-cn` / `en-us`…） |
| `--play` | 开放模式：由数据入口页驱动，不限帧数 |
| `--frames N` | 推 N 帧（= N 个逻辑步）后收工并打摘要；`0` = 不限 |
| `--delta <秒>` | 覆盖 `_process` 节拍（只决定每帧给累加器加多少毫秒，不改固定步长） |
| `--fighter <id>` / `--players N` | 指定出场角色 / 人数（验收模式） |
| `--stage <id>` / `--vs` | 指定关卡 / 对抗模式（验收模式） |
| `--click-window <x>,<y>[@frame]` | 窗口像素的点击（走引擎事件路径） |
| `--click <x>,<y>[@frame]` | 逻辑像素的点击（直接喂游戏 UI） |
| `--hover-window <x>,<y>[@frame]` | 只推一条鼠标移动（悬停态） |
| `--key` / `--key-down` / `--key-up` / `--key-frame` / `--key-tap` | 脚本化按键，语义同 headless |
| `--window-scale N` / `--window-size WxH` | 窗口尺寸（`--play` 缺省 1.5 倍） |
| `--titlebar <native\|none\|custom\|frame>` | 标题栏方案（`--play` 缺省 `frame`） |
| `--icons <dir>` / `--display-scale N` | 自绘标题栏的图标目录 / 显示缩放 |
| `--screenshot <png>` / `--shot-frame N` | 存整窗截图（不要配 `--headless`） |
| `--ui-page <id>` | 装载后把这一页设到第 0 层 |
| `--ui-dump` / `--clip-probe` | 诊断输出（逐节点事实 / 画面边界探针） |
| `--ui-scroll <id>:<px>` | 把某个 `ScrollView` 节点滚到指定偏移（验收截断 / 裁剪用） |
| `--sync-render <0..4>` | 强制渲染频率模式：0 无限 / 1 减半 / 2 同步 / 3 60FPS / 4 120FPS（缺省取数据集） |
| `--text-input <text>[@frame]` | 脚本化打字 |

> 节拍：`--delta` / 引擎 `p_delta` 只决定「每帧给累加器加多少毫秒」，真正的固定步长是
> 世界当前的 `base_step_ms()`（`1000 / UPS / playrate`），每帧重算。所以在游戏里把 UPS 切到
> 30、或按 `F5` 加速都会立刻生效，不会停在启动时的步长上（30UPS 下每个 tick 前进 2 个 atom，
> 步长没跟着变就会变成两倍速）。
>
> 帧率：`sync_render`（设置页「帧率」那一行）只管**渲染**节奏，和 UPS 解耦——`同步` = 每个 tick
> 渲染、`减半` = 隔一个 tick、`60/120 FPS` / `无限` = 由帧回调按目标帧率补渲染（30UPS + 60FPS
> 就是仿真 30 次/秒、画面 60 次/秒）。验收用 `--sync-render <0..4>` 覆盖。

## 性能探针

帧级耗时统计（逻辑步各段 + 渲染管线各段）默认**不编译进来**，由宏 `LFW_PERF` 控制
（头文件 `lfw/base/perf.h`，关闭时所有 `LFW_PERF_*` 宏展开成空语句）：

```powershell
# 打开：CMake 选项 LFW_GODOT_PERF 会给 lfw_core / lfw_godot 都加上 -DLFW_PERF=1
$env:LFW_GODOT_PERF = "1"; npm run godot:build
# 跑完之后（`finish()` 里）多打两行 [perf]
node tools/native.mjs godot-run --headless -- --ui-page test_cases_page --key a@20 --frames 600
# 压力场：test_cases_page 的用例来自 directors（只有 football 标了 platform="godot"），
# `a` 起 100v100 用例，`r` 切下一条（200v200）。实体数会涨到 1700+（一路生成的武器/飞行道具），
# 这时 step 里的宽相位和实体更新才会真正吃满（单 tick 30ms+）。
node tools/native.mjs godot-run --headless -- --ui-page test_cases_page --key a@10 --key r@200 --frames 1400
```

| | 说明 |
|---|---|
| `LFW_GODOT_PERF=1` / `-DLFW_GODOT_PERF=ON` | 打开探针（缓存变量；要关就 `=0` 再编一次） |
| `LFW_PERF_SCOPE(slot)` | 给当前作用域计时（RAII） |
| `LFW_PERF_LAP(name, slot)` … `name.stop()` | 长函数里手动圈一段 |
| `LFW_PERF_SAMPLE(name, slot)` … `name.stop()` | 同上，但每 16 次只测 1 次（`kPerfSampleMask`）再按比例还原；给「每帧被调用上万次」的细粒度段用，免得探针本身成为开销 |
| `LFW_PERF_COUNT / ADD` | 计数（帧数、draw 调用数、贴图查询次数…） |
| `perf::set_now(fn)` | 墙钟由**宿主**注入（核心库不许用 `<chrono>`，见 lint） |

输出形如：

```
[perf] per-frame ms: step=26.98 render=2.85 assign=0.21 draw=2.81 world_ent=9.54 world_broad=15.67 ...
[perf] counts: processes=589 steps=600 renders=589 draws=589 items=1369607 draw_ops=5103587 ...
[perf] wall=4208.70ms frames=600 render_cost=0.00ms frame=35.11ms engine=3.85ms
```

分段名与含义见 `lfw/base/perf.h` 的 `PerfSlot` 注释（`world_broad` / `world_ent` / `render_build` /
`render_order` / `render_emit` / `draw` …）。`other` = 墙钟时间减去顶层分段（`step + render +
assign + sync_text + draw`），用来确认没有漏计的大块。

**看 CPU 用 `frame_total`，别看 `total`**：`total`（= `wall / frames`）是两帧 `_process` 之间的墙钟，
headless 下它被显示刷新率钉住（本机 164Hz ⇒ 空转也有 ≈6ms 的 `engine`，跑 2 个角色和跑 1800 个实体
都一样）。真正属于宿主回调的时间是第三行的 `frame_total`（`_process` 从头到尾），
`engine` = `total - frame_total`（引擎主循环 / 等垂直同步，不是我们的代码）。

常用分段（由外到内，父段包含子段）：

| 段 | 含义 |
|---|---|
| `step` | 一个逻辑 tick（`World::step`），下面这些都嵌在它里面 |
| `world_ent` | `Entity::update` 循环；`ent_timers`（含 `ent_recover_hp`）→ `ent_frame` → `ent_physics` → `ent_ctrl`（含 `ent_ctrl_upd` / `ent_catch`）→ `ent_tail` |
| `ent_recover_hp` | 回血 + 帧上的 hp 流失；`ent_hp_data` = 取 `hp_r_*` 数据，`ent_hp_set` = 回血写 `set_hp`，`ent_hp_frame` = 帧流失写 `set_hp` |
| `set_hp_summary` / `set_hp_cb` / `set_hp_dead` | `set_hp` 里扣血记分（`SummaryMgr`）、回调、死亡分支 |
| `world_broad` | 宽相位扫掠 + 碰撞；`broad_lookup` = bot/球每 `kLookupUpdateInterval` 帧的目标搜索，`broad_get` = 逐对的 `collision_get`（内部 `col_prep` / `col_test`，`col_test` 里再分 `col_precheck` / `col_commit_t`） |
| `render_build` / `render_ui` | 生成绘制项（世界 + UI 层） |
| `render_order` / `render_emit` | 绘制项排序 / 逐个发射（`tag`、贴图解析、姿态插值） |
| `draw` | `_draw` 里把所有绘制项提交给 `RenderingServer` |
| `ent_ctrl_upd` 内层 | `bot_all` = `BotController::update` 全程（内含 `bot_check` / `bot_prune` / `bot_sort` / `bot_fsm`（再分 `bot_s_idle` / `bot_s_chase` / `bot_s_avoid` / `bot_s_follow`，以及 `bot_hba` / `bot_hdef` / `bot_desire`）/ `bot_base`）；`ball_all` = `BallController::update`（`ball_chase` = 追敌那一支）；`ent_nf` = 控制器 `nf` 命中后的 `handle_next_frame_result`；`bot_keys` = `start/end/key_down/key_up` 本体（按 `key_ops` 计） |

## 测试语料

`tests/differential/run.mjs` 是原先的差分调度器（自动发现两侧 subject 跑同一 case）。
TS 对照侧已移除，现在只保留用例语料与协议说明：`tests/differential/PROTOCOL.md`。
