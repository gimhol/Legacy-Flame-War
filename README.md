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
resources/icons/          自绘标题栏的 btn_*.png
```

宿主启动时若没给 `--root` / `--zip` / `--icons`，会自动取 `<exe 所在目录>/resources/...`；
命令行显式给的优先。双击时工作目录就是包目录，所以工程和资源都找得到。

## 文档

命令与参数的完整清单见 [docs/commands.md](docs/commands.md)。
