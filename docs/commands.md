# 命令参考

打包与运行时的专题文档： [打包](packaging.md) · [数据包与压缩](data-packages.md) ·
[模组](mods.md)。

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

| 参数 | 说明 |
|---|---|
| `--headless` | 无窗口（只打摘要；截图会得到空白） |
| `--root <dir>` | 资源根目录，可重复；缺省 `.` |
| `--zip <dir>` | 解包后的数据包目录或 `.zip` 文件，可重复（zip 只读索引 + 用到的条目，整包不进内存） |
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
