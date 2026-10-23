# 游戏内控制台

Ctrl + ` 切换控制台

## 操作

| 按键 | 作用 |
|---|---|
| `Enter`（含小键盘） | 执行当前输入 |
| `↑` / `↓` | 上一条 / 下一条历史（回到最新一条后继续按 = 回到没提交的草稿） |
| `Esc` | 关闭控制台 |
| `Ctrl+\`` | 开 / 关（开着时再按也关） |
| 输入 `clear` / `cls` | 清空日志（不会发给游戏） |

控制台打开期间键盘与鼠标**都不进游戏**（点击画面其它位置也会把焦点还给输入框），关闭时会把
游戏侧还在按下的键统一 `key_up`，不会出现“一直按着”的粘键。

## 命令

命令集就是游戏自己的 `CMDS`（与数据包 / 模组里用的同一套），命令名**大小写不敏感**：

```
help                   # 列出所有命令（4 列，共 37 条）
help bgm               # 显示 BGM 的完整帮助
bgm --gain=0.2         # 播 BGM
CHANGE_STAGE tutorial  # 切关卡（参数值大小写敏感）
SET_DIFFICULTY 3       # 1..4
KILL_ENEMIES
```

参数名（`--gain=`）与位置参数取值都按原文精确匹配。命令在下一次世界步进时执行（世界暂停时
也能跑，因为 `handle_cmds()` 在暂停判断之前）。

`help` 的输出走宿主的 `log` 通道（控制台里是白色行，不增加摘要里的 `warns` 计数）：

```
37 commands:
  BGM               CHANGE_BG         CHANGE_STAGE      DEL_PUPPET
  DESPAWN           DIST_CAM          F1                F10
  ...
（`help <command>` 看某条命令的完整帮助）
```

## 诊断参数

| 参数 | 作用 |
|---|---|
| `--console` | 启动时就把控制台打开（截图 / 验收用） |
| `--console-exec <行>` | 启动后按控制台输入执行这条命令，可重复；日志里打 `[godot] console: <行>` |

```bash
node tools/native.mjs godot-run --console --frames 120 \
  --screenshot shot.png --console-exec "SET_DIFFICULTY 9"
```

## 日志来源

面板里的非回显行来自宿主回调：`log` → 白、`warn` → 橙、`error` → 红（`LfwGodotHost` 覆写
`warn` / `error` / `log` 后转发给控制台，同时照旧打到 stdout）。最多保留 300 行。
