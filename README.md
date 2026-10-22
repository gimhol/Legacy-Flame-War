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

## 打包

```bash
npm run godot:pack        # -> release/Legacy Flame War_godot_<版本>/
npm run godot:pack:zip    # 同上，外加同名 .zip
```

产物免安装、零参数：双击 `lfw.exe` 即玩，数据包 / 模组目录 / 图标都在包内。脚本自己推导
所有输入，不需要传路径，也不需要 export templates。

## 文档

| 文档 | 内容 |
|---|---|
| [docs/commands.md](docs/commands.md) | 命令与参数清单、打包产物布局、模组语义、图标与启动画面 |

