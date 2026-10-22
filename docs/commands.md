# 命令参考

## 前置

| 需要 | 说明 |
|---|---|
| CMake ≥ 3.20 + Ninja | `build/msvc-x64` 用 Ninja 生成器 |
| C++20 编译器 | Windows 上默认找 MSVC 2022；其它平台用 `CMakePresets.json` 的预设 |
| Node.js | `tools/` 下的脚本都是 ESM |

只编 Godot 宿主时另需：

| 需要 | 说明 |
|---|---|
| Godot 4.7 | 跑 `godot-run`、dump API |
| godot-cpp 检出 | 环境变量 `GODOT_CPP_DIR` 指向它 |
| `build/godot-api/extension_api.json` | 由本机 Godot 生成，见下 |

```bash
<godot-console> --headless --dump-extension-api --dump-gdextension-interface
# 把生成的 extension_api.json 放到 build/godot-api/
```

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
→ 桌面上的官方构建。

## 打包（`tools/package-godot.mjs`）

```
node tools/package-godot.mjs [--zip] [--unpack]
```

### 输入与产物

**没有路径参数**：所有输入都在脚本里推导 —— Godot 运行时（`GODOT_BIN` → `PATH` → 桌面）、
`host/godot/project`（工程 + `bin/lfw_godot.dll`）、`dist/`（`builtin_data` + `version.json`）、
`dist/{prel,data}.zip`（`--unpack` 时改成解包目录）、`src/assets/btn_*.png`。

产物是 `release/Legacy Flame War_godot_<版本>/`（`--zip` 时再加同名 `.zip`）：

| 路径 | 内容 |
|---|---|
| `lfw.exe` | 本机 Godot 运行时的副本，双击即玩 |
| `project.godot` / `main.tscn` / `lfw_godot.gdextension` | 工程（包根目录就是 `res://`） |
| `bin/lfw_godot.dll` | GDExtension |
| `.godot/extension_list.cfg` | 扩展注册表（缺了启动时 `LfwGodot` 类找不到） |
| `resources/root/` | `dist/builtin_data` + `version.json` |
| `resources/prel.zip` `resources/data.zip` | 数据包（宿主直接读 zip；`--unpack` 则是解包目录） |
| `resources/icons/` | 自绘标题栏的 `btn_*.png` + 窗口图标 `icon.ico` / `icon.png` |
| `mods/` | 模组目录：放 `*.zip` 启动时自动加载（含 `readme.txt`） |

不需要 export templates：用本机 Godot 运行时直接跑工程目录。

### 零参数启动

宿主没收到 `--root` / `--zip` / `--icons` 时会自动取
`<exe 所在目录>/resources/{root,prel[.zip],data[.zip],icons}`（命令行显式给的优先）；双击
`lfw.exe` 时工作目录就是包目录，所以工程和资源都能找到。开发时 `npm run godot` 自己补
`--root dist`、`--zip build/host-data/{prel,data}` 与 `--mods native/mods`。

### 去 Godot 标注

| 位置 | 做法 |
|---|---|
| 启动画面 | `project.godot` 的 `boot_splash/show_image=false` + 黑底 |
| 窗口标题 | 宿主首帧用 Win32 `SetWindowTextW` 改成 `Legacy Flame War`（`Window.set_title` 会被引擎追加 `" (DEBUG)"`） |
| 窗口 / 任务栏图标 | `resources/icons/icon.ico` 优先（Win32 `WM_SETICON`，Godot 的 `Image` 不认 ICO），其次 `icon.png`（`DisplayServer.set_icon`），都没有就是 Godot 默认图标 |
| `lfw.exe` 文件图标 | 仍是 Godot 运行时的 PE 资源，改它需要额外工具 |

图标源文件优先取 `host/godot/project/icon.(ico|png)`（或 `native/icon.*`），没有就用
`extension/icon256.png` 并自动包出同名 `.ico`。换图标 = 把 `icon.ico` 放进
`host/godot/project/` 重打包，或直接替换已打包目录里的 `resources/icons/icon.ico`。

### 模组

语义和 Electron 端一致。默认目录（越后面优先级越高）：**`<exe>/mods` → `<cwd>/mods` →
用户数据目录 `mods/`（`user://mods`）**，`--mods <dir>` 再追加，`--no-mods` 关掉。只认目录下
的 `*.zip`（不进子目录）。

两种 zip：

- **普通数据包**：与本体合并（同名资源以模组为准），入口页「额外数据」会列出来；
- **自定义游戏包**：内含 `index.json` / `index.json5`，`paths` 指向包内 `prel.zip` / `data.zip`
  等，整包替换本体数据，多个时只认第一个（按文件名排序）。

入口：

| 入口 | 行为 |
|---|---|
| 启动扫描 | 依次加载默认目录与 `--mods` 里的 zip |
| 拖 zip 进窗口 | 普通数据包热加载（同名去重，「额外数据」同步刷新）；自定义包自动重启换成它 |
| 入口页「附加模组」 | 弹系统文件对话框，选中的 zip 走同一条热加载路径 |
| 入口页「自定义游戏」 | 选 **full.zip**，暂存到 `user://custom-game/pending.zip` 后自动重启加载（只对本次会话生效），对应 Web 端 `/custom_game` |

目录里可放 `mods.json5` / `mods.json` 控制顺序与禁用：

```json5
{ order: ["基础模组.zip", "覆盖补丁.zip"], disabled: ["先别用的模组.zip"] }
```

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
| `--zip <dir>` | 解包后的数据包目录，可重复，按给定顺序 load |
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

| 参数 | 说明 |
|---|---|
| `--headless` | 无窗口（只打摘要；截图会得到空白） |
| `--root <dir>` | 资源根目录，可重复；缺省 `.` |
| `--zip <dir>` | 解包后的数据包目录或 `.zip` 文件，可重复 |
| `--mods <dir>` | 额外模组目录（放 `*.zip`），可重复；比默认目录后加载（优先级更高） |
| `--drop <zip>` | 诊断：启动后按「拖拽」路径把一个 zip 当模组热加载（和窗口里拖入同一条信号链），可重复 |
| `--custom-game <zip>` | 诊断：启动后走「自定义游戏」路径（暂存 + 自动重启）加载一个 full.zip |
| `--no-mods` | 不自动加载模组（默认目录与 `--mods` 都跳过） |
| `--lang <code>` | UI 语言码（`zh-cn` / `en-us`…） |
| `--play` | 开放模式：由数据入口页驱动，不限帧数 |
| `--frames N` | 推 N 帧后收工并打摘要；`0` = 不限 |
| `--delta <秒>` | 覆盖 `_process` 节拍（不改固定步长） |
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
| `--text-input <text>[@frame]` | 脚本化打字 |

## 测试语料

`tests/differential/run.mjs` 是原先的差分调度器（自动发现两侧 subject 跑同一 case）。
TS 对照侧已移除，现在只保留用例语料与协议说明：`tests/differential/PROTOCOL.md`。
