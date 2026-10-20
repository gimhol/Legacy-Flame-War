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

## 文档

命令与参数的完整清单见 [docs/commands.md](docs/commands.md)。
