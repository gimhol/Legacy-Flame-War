# 游戏内控制台

Ctrl + ` 切换控制台

## 操作

| 按键 | 作用 |
|---|---|
| `Enter`（含小键盘） | 执行当前输入 |
| `↑` / `↓` | 上一条 / 下一条历史（回到最新一条后继续按 = 回到没提交的草稿） |
| `PgUp` / `PgDn`、滚轮 | 往回翻 / 往回翻到底（新行只在贴底时跟随） |
| 右侧滚动条 | 日志超出一屏时出现：可拖动 / 点轨道翻页 / 在条上滚轮 |
| `Esc` | 关闭控制台 |
| `Ctrl+\`` | 开 / 关（开着时再按也关） |
| 输入 `clear` / `cls` | 清空日志（不会发给游戏） |

控制台打开期间键盘与鼠标**都不进游戏**（点击画面其它位置也会把焦点还给输入框），关闭时会把
游戏侧还在按下的键统一 `key_up`，不会出现“一直按着”的粘键。

## 布局

面板铺满整个窗口，**输入框永远钉在窗口最下面一行**（窗口怎么缩放都贴底）；日志行从输入框
往上排，能显示多少行就显示多少行（按窗口高度算，没有固定行数上限），超出后靠 `PgUp`/`PgDn`/
滚轮/右侧滚动条翻看，最多保留最近 2000 行。滚动条只在日志超过一屏时出现，滑块大小 =
可见行数 / 总行数。

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
43 commands:
  BGM               CHANGE_BG         CHANGE_STAGE      DEL_PUPPET
  DESPAWN           DIST_CAM          ENTITIES          ENTITY
  F1                ...
（`help <command>` 看某条命令的完整帮助）
```

## 查询命令

输出同样走 `log`（白色）。

| 命令 | 作用 |
|---|---|
| `LIST <kind> [filter]` | 列某一类数据的 id + 名称；`filter` 按 id / 名称做子串匹配 |
| `ENTITIES [--team=<t>] [--all]` | 列场上实体：id、数据 id、队伍、hp/mp、坐标 |
| `ENTITY <id> [--<prop>=<value> ...]` | 看单个实体；带选项就先改再显示（见下节） |
| `INFO` | 当前世界概要：关卡、背景、难度、暂停、相机、实体数、玩家、语言、版本 |
| `UI [layer] [--depth=<n>]` | 列 UI 各层，或某一层的节点树（子节点下标正是 `POINTER_*` 的 `--paths`） |

`LIST` 的 kind：`fighters` `weapons` `balls` `objects` `entities` `bots` `moves` `stages`
`bg`（=`backgrounds`）`directors`。标题里的 `(5/28)` = 命中 / 总数。

```
> LIST stages 1-
stages (5/28):
  0  1-1
  1  1-2
  ...

> ENTITIES --all
entities (2):
  #101 gim "Gim"  team=1  hp=500/500  mp=500/500  pos=(21,0,-90)

> INFO
stage       tutorial (TUTORIAL)
bg          ash_forest (Ash Forest)
difficulty  3
paused      false
camera      (0,0)
entities    2  ghosts=0  puppets=2
players     1, 2
lang        zh-cn
version     v0.1.62-...

> UI
ui layers (1):
  [0] init  pages=1  nodes=13
（`ui <layer>` 看节点树，`--depth=<n>` 调深度）
```

`UI <layer>` 的树按 `下标 名字 (子节点数)` 缩进，`[hidden]` / `[disabled]` 直接标在行尾；
想对某个节点发 `POINTER_CLICK`，把沿线下标拼成 `--paths=<layer>,<pageIdx>,<i>...` 即可。

### `ENTITY`：看 / 改单个实体

`id` 用 `ENTITIES` 打出来的那个（`#101`，`#` 可省）。不带选项就是只读；带了选项会先把改动
应用掉，再把改后的状态打印出来——一条命令既能当探针也能当调试手柄。

```
> ENTITY #101
entity #101
  data      gim
  name      "Gim"  visible=1
  team      1  group=Dev
  position  (21,0,-90)  prev=(-80,0,0)
  velocity  (0,0,0)  prev=(0,0,0)
  frame     0  state=0  wait=7
  hp/mp     500/500  500/500
  facing    1  on_ground=1
  flags     invisible=0  invulnerable=144  shaking=0  motionless=0  arest=0  reserve=0
  other     ctrl=bot  puppet=1  ghosted=0  buffs=0  lifetime=0
  link      holding=-  bearer=-  catching=-  catcher=-

> ENTITY #101 --x=300 --hp=250 --shaking=8 --frame=walking_0
（先应用改动，再打一份同样的表）
```

| 选项 | 作用 |
|---|---|
| `--x/--y/--z=<n>` | 位置；只给其中一个时其余不变。`set_position` 会按场地边界夹住并刷 `ground_y`，所以越界的 z 会看到被拉回边界值 |
| `--velocity=<x,y,z>` | 速度，`prev_velocity` 一起改（免得下一帧插值跳一下） |
| `--frame=<frame_id>` | 切帧；数据里没有这个 id 直接 warn（不会像数据脚本那样悄悄落到 `auto` 帧） |
| `--state=<code>` | 设状态码（数值，与帧数据里的 `state` 同一套）；表里没有就 warn |
| `--name=<text>` / `--name_visible=<0\|1>` | 显示名（空 = 回到数据里的名字）/ 是否显示名字 |
| 其余 `--k=v` | 走 `A_SET_PROP` 那套：`hp hp_r hp_max mp mp_max invisible invulnerable toughness fallinjury throwinjury motionless shaking facing arest dropping team reserve`；不认识的名字 warn，不会静默吞掉 |
| `--holding=<id>` / `--bearer=<id>` | 把两个实体的引用连起来（`--holding=#102` = 102 拿在手上），两边的反向引用一起设好；写空 / `none` = 断开 |

## 语言切换

`SET_LANG <code>`（`zh-cn` / `en-us` …）可以在运行时换语言，当前页面不用重开：

1. `host.lang_apply()` 换 I18N 表；
2. 逐层逐页遍历 UI 树，`UINode::refresh_lang_text()` 把文本按新语言重解析——数据里带
   `i18n` 的节点按词条重算（大小随新文本重测），纯字面词条节点按新语言重新量一遍；
3. 抛 `on_lang_changed` 回调。

设置页里的语言开关走的是同一入口，所以那一页自身也会立刻跟着变。

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
`warn` / `error` / `log` 后转发给控制台，同时照旧打到 stdout）。最多保留 2000 行。

## 代码位置

控制台自成一个类：`host/godot/lfw_godot_console.{h,cpp}`（`lfw::host::GodotConsole`），
输入框 / 滚动条 / 日志绘制 / 历史 / 滚动都在里面；`lfw_godot_node.cpp` 只做接线——把
「执行命令」（`LFW::push_cmd`）、「取字体」（`font_for_css`）、开 / 关时的副作用（释放游戏按键、
让出 UI 输入焦点）用回调注入进去。
