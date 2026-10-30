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

## 文档

| 文档                                          | 内容                                                              |
| --------------------------------------------- | ----------------------------------------------------------------- |
| [docs/commands.md](docs/commands.md)           | 前置依赖、npm scripts、`native.mjs` 子命令、宿主参数清单        |
| [docs/packaging.md](docs/packaging.md)         | 打包产物布局、零参数启动、去 Godot 标注（标题 / 图标 / 启动画面） |
| [docs/data-packages.md](docs/data-packages.md) | 数据包惰性读取、支持的压缩方法、解压缓存与完整性校验              |
| [docs/remote-data.md](docs/remote-data.md)     | 远端数据包（HTTP 下载、Range 断点续传、`user://` 落盘缓存、版本检查） |
| [docs/mods.md](docs/mods.md)                   | 模组目录、普通数据包与自定义游戏包、拖拽 / 对话框入口             |
| [docs/console.md](docs/console.md)             | 游戏内控制台（Ctrl+`）：命令输入、日志、诊断参数                  |
