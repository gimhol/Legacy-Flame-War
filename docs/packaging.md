# 打包（`tools/package-godot.mjs`）

```
node tools/package-godot.mjs [--zip] [--unpack]
```

`--zip` 额外压一个同名 zip（`npm run godot:pack:zip`），`--unpack` 把数据包展开成目录
（缺省是直接把 `dist/*.zip` 放进包内）。

## 输入与产物

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

数据包含义与读取方式见 [data-packages.md](data-packages.md)，模组见 [mods.md](mods.md)。

## 零参数启动

宿主没收到 `--root` / `--zip` / `--icons` 时会自动取
`<exe 所在目录>/resources/{root,prel[.zip],data[.zip],icons}`（命令行显式给的优先）；双击
`lfw.exe` 时工作目录就是包目录，所以工程和资源都能找到。开发时 `npm run godot` 自己补
`--root dist`、`--zip build/host-data/{prel,data}` 与 `--mods native/mods`。

## 去 Godot 标注

| 位置 | 做法 |
|---|---|
| 启动画面 | `project.godot` 的 `boot_splash/show_image=false` + 黑底 |
| 窗口标题 | 宿主首帧用 Win32 `SetWindowTextW` 改成 `Legacy Flame War`（`Window.set_title` 会被引擎追加 `" (DEBUG)"`） |
| 窗口 / 任务栏图标 | `resources/icons/icon.ico` 优先（Win32 `WM_SETICON`，Godot 的 `Image` 不认 ICO），其次 `icon.png`（`DisplayServer.set_icon`），都没有就是 Godot 默认图标 |
| `lfw.exe` 文件图标 | 仍是 Godot 运行时的 PE 资源，改它需要额外工具 |

图标源文件优先取 `host/godot/project/icon.(ico|png)`（或 `native/icon.*`），没有就用
`extension/icon256.png` 并自动包出同名 `.ico`。换图标 = 把 `icon.ico` 放进
`host/godot/project/` 重打包，或直接替换已打包目录里的 `resources/icons/icon.ico`。
