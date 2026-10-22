# Legacy Flame War

2D 横版格斗游戏，C++ 实现。

## 构建

```bash
npm run native configure
npm run native build
```

## 运行

```bash
npm run godot
```

运行时需要一个解包好的数据目录，用 `--zip` 传入（可重复）：

```bash
npm run godot -- --zip <数据目录>
```

## 打包（免安装目录）

```bash
npm run godot:pack        # -> native/release/Legacy Flame War_godot_<版本>/
npm run godot:pack:zip    # 同上，外加同名 .zip
```

脚本自己推导所有输入（Godot 运行时、`host/godot/project`、`dist/`、`src/assets`），
**不需要传路径**，也不需要 export templates（用本机 Godot 运行时直接跑工程）。数据包默认
原样放 `dist/{prel,data}.zip`（宿主直接读 zip），加 `--unpack` 才展开成目录。产物：

```
start.exe                 Godot 运行时，双击即玩（零参数）
project.godot / main.tscn 工程（包根目录就是 res://）
bin/lfw_godot.dll         GDExtension
.godot/extension_list.cfg 扩展注册表（缺了 LfwGodot 类找不到）
resources/root/           dist/builtin_data + version.json
resources/prel.zip|data.zip 数据包（宿主直接读 zip；`--unpack` 则解包成目录）
resources/icons/          自绘标题栏的 btn_*.png + 窗口图标 icon.(ico|png)
mods/                      模组目录（放 *.zip 启动时自动加载，可选 mods.json5 控制顺序/禁用）
```

宿主启动时若没给 `--root` / `--zip` / `--icons`，会自动取 `<exe 所在目录>/resources/...`；
命令行显式给的优先。双击时工作目录就是包目录，所以工程和资源都找得到。

启动画面不显示 Godot 的 logo（`project.godot` 的 `boot_splash/show_image=false` + 黑底），
窗口标题固定成 `Legacy Flame War`（宿主用 Win32 改，绕开引擎给自己追加的 `" (DEBUG)"`），
任务栏/窗口图标取 `resources/icons/icon.ico`（没有就 `icon.png`；两个都没有就是 Godot 默认
图标）。打包时优先用工程里的 `host/godot/project/icon.(ico|png)`，没有就从
`extension/icon256.png` 复制一份 PNG 并顺手包出同名 `.ico`；**想换图标就把 `icon.ico` 丢进
`host/godot/project/`（或直接替换包里的 `resources/icons/icon.ico`）**。
注意这改的是**窗口 / 任务栏**图标；`start.exe` 文件本身的图标仍来自 Godot 运行时（改 PE
资源要另外的工具），没有 Godot 标注的只有启动画面、窗口标题和窗口图标这三处。

模组语义和 Electron 端一致：`mods/*.zip` 自动加载。默认目录（越后面优先级越高）是
`<exe>/mods` → `<cwd>/mods` → 用户数据目录 `mods/`（Godot 的 `user://mods`），`--mods <dir>`
再追加，`--no-mods` 关闭。目录里可放 `mods.json5` / `mods.json` 的 `order` / `disabled`
控制顺序与禁用。

两种 zip 都支持，和 Electron 端一样：

- **普通数据包**：与本体合并（同名资源以模组为准），入口页「额外数据」会列出来；
- **自定义游戏包**（内含 `index.json` / `index.json5`，`paths` 指向包内的 `prel.zip` / `data.zip` 等）：
  整个游戏被它替换，多个时只认第一个（按文件名排序）。

另外，**把 `*.zip` 直接拖进游戏窗口**也会当模组热加载（普通数据包，同名去重，加载后「额外数据」
列表同步刷新）；拖入自定义游戏包会自动重启并换成该包。入口页的「附加模组」按钮会弹系统文件
对话框，选中的 zip 走同一条热加载路径（和 Web 端 `select_extra_data` → `open_file` 一致）。

入口页的「自定义游戏」也是弹文件对话框，但选的是 **full.zip**（内含 `index.json` / `index.json5`，
`paths` 指向包内的 `prel.zip` / `data.zip` 等）：整包替换本体数据（对应 Web 端 `/custom_game` 页）。
因为运行中换不了数据源，宿主会把包暂存到 `user://custom-game/pending.zip` 然后**自动重启**；
新进程加载完就删掉，所以和 Web 端一样只对本次会话生效。

## 文档

命令与参数的完整清单见 [docs/commands.md](docs/commands.md)。
