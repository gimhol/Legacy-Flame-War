# 联机（netplay）

> 状态：**可联机（游客模式）**。WebSocket 传输、客户端会话、房间/聊天 UI、Tick 帧驱动都已接进游戏；
> 账号登录、`Dataset` 广播、重连回放重放、数据包一致性校验仍待补。
> 自检：`node tools/native.mjs godot-run -- --headless --net-selftest --frames 5`

## 1. 现状

| 层 | Web/TS | native（本仓库） |
|---|---|---|
| 协议 | `src/Net/**`，WS 文本帧 + JSON 信封 `{is_req,pid,type,…}` | **已移植**：`host/net/net_client.{h,cpp}`（信封/请求响应/超时/房间/Ping/重连） |
| 服务端 | `server/`（Node + ws）：房间、帧聚合、重连回放 | 复用 Web 端服务器（native 直连） |
| 同步策略 | `src/Net/net_sync.ts`：`recommend_sync` / `resolve_sync` | **已移植**：`host/net/net_sync.{h,cpp}` |
| 帧驱动 | `LockstepNetworkDriver` / `DelayNetworkDriver` | **已移植**：`host/net/net_frame_driver.{h,cpp}`；`LfwGodotHost` 已接线 |
| 传输 | `WebSocket` | **Godot `WebSocketPeer`**：`host/godot/lfw_godot_ws.{h,cpp}`；另有回环实现供自检 |
| 确定性底座 | 固定 60UPS + `mt.reset(seed)` | **已接**：`RoomStart.seed` → `mt.reset`，60UPS/atom_time/按键参数按 Web 端固定 |
| UI | 内嵌 UI（连接/房间列表/房间/浮层/聊天） | **已移植**：`NetworkLogic`（单例）+ `ChatLogic` + `chat_store/chat_overlay` |
| Steam | — | 只到 `init/poll/overlay`（`host/godot/lfw_godot_steam.{h,cpp}`） |

## 2. 已落地（本仓库）

```
host/net/
  net_sync.{h,cpp}              同步策略：NET_UPS=60, LOCKSTEP_RTT_RATIO=1.25,
                                RTT_JITTER_MARGIN=1.3, MIN_LEAD=2, MAX_LEAD=8, UNKNOWN_RTT=100
  net_protocol.{h,cpp}          信封 + Tick 请求/响应 + RoomStart 的 JSON 编解码（字段与 Web 对齐）
  net_transport.h               传输抽象（connect/poll/drain/send/connected/rtt_ms）
  net_transport_loopback.{h,cpp} 进程内回环 + 固定 RTT 投递（时间由测试推进，结果可复现）
  net_frame_driver.{h,cpp}      IFrameDriver / LockstepDriver(lead=1) / DelayDriver(lead=K)
  net_client.{h,cpp}            客户端会话：请求/响应、房间状态、Ping、重连、Tick 分发
  net_selftest.{h,cpp}          自检：策略表、协议往返、两种驱动的回环调度

host/godot/
  lfw_godot_ws.{h,cpp}          WebSocket 传输（`WebSocketPeer`）

lfw/net/
  net_service.h                 UI 使用的 `INetService` / `INetListener`
  chat_store.{h,cpp}            聊天内容 + 发送通道
  chat_overlay.{h,cpp}          聊天层（layer 2）+ `T` 热键

lfw/ui/component/
  network_logic.{h,cpp}         四个联机页面（弹窗/列表/房间/浮层）
  chat_logic.{h,cpp}            聊天浮层渲染与预览淡出
```

自检覆盖：`rtt→{lockstep,delay}` 判定表、`Tick` 请求/响应与 `RoomStart` 的往返、
2 客户端在 4ms/80ms RTT 下各跑 120/300 帧且**两端命令流逐字一致、零丢帧**。

`--net-selftest` 只打日志、不改变任何游戏行为（60 帧回归 digest 不变）。

## 3. 一局的时序（与 Web 端一致）

```
C→S ClientInfo{name,players}          S→C ClientInfo{client,secret}      // 注册 + 每 500ms Ping
C→S CreateRoom/JoinRoom               S→C 广播 room{clients,owner,started,sync_mode}
C→S ClientReady{ready}                S→C 广播 ClientReady{client,ready}
C(owner)→S RoomStart{}                S→C RoomStart{seed, sync_mode, input_delay}
各 C→S Tick{seq:0}                    S→C Tick{seq:0, reqs[]}            // 收齐所有在线客户端才广播
稳态：每帧 C→S Tick{seq = 已执行+lead, cmds[], _s}  →  S→C Tick{seq, reqs[], bot_events?}
```

- `reqs` 的**数组顺序**就是各端执行命令的顺序（服务端按客户端顺序拼），任何端都不许自行重排。
- `cmds` 是 LFW 命令字符串，末尾带 `--from=<client_id>`；输入注入复用核心现成的
  `KEY_EVENT --p=… --s=… --c=… --n=…` 路径（`lfw/cmds/cmd_key_event.cpp`：
  UI 层 → `mounted_keys` 的 `KeyStatus` → 玩家的 `ctrl->start/end`），不必改控制器。

## 4. 同步模型

```
tick_ms = 1000/UPS                                     // 60UPS → 16.667ms
rtt     = max_rtt || 100                               // 0/未测到按 100ms 估
lockstep  ⟸ rtt <= tick_ms * 1.25                      // ≈20.8ms
delay     ⟸ 否则，K = clamp(ceil(rtt * 1.3 / tick_ms), 2, 8)
```

| | lockstep | delay |
|---|---|---|
| 提前提交 | 1 帧（执行第 N 帧时提交 N+1） | K 帧 |
| 推进门控 | 收到第 N 帧才跑第 N 帧，跑完即停 | 有缓冲就跑，缺帧才停 |
| 抖动容忍 | 0 | K 帧 |
| 代价 | 一帧晚到全员停 | K 帧操作延迟 |

`IFrameDriver::submit_seq() = _seq + lead() - 1`，`start()` 后先补发 `lead` 帧
（`lead=1` 即只补第 0 帧），之后每执行一帧补一帧，窗口恒定。

## 5. 接线点（接传输 / 接世界时改这里）

| 用途 | 位置 |
|---|---|
| 每帧收包 | `LfwGodot::_process` 里 `_audio.poll()` / `_steam.poll()` 旁边（现在只跑了 `--net-selftest`） |
| 步进闸门 | `lfw_godot_node.cpp` 的 `while (… can_step())`：联机时改成「本帧输入齐了才 `push_frame`」 |
| 一步 = 一帧 | `LfwGodotHost::push_frame`：`clock.advance(step); world.update_once(step); ++steps`（`steps()` 天然是 `seq`） |
| 远程输入 | 复用 `cmd_key_event` 的三段式注入；需要补「`player_id` → 键位所有者」的映射 |
| 种子 | `LFW::LFW(...) : _mt(host.now())`（`lfw.cpp`）—— 需要能被 `RoomStart.seed` 覆盖 |
| 状态摘要 | `lfw/core/state_hash.h` + `host/common/lfw_boot.cpp::frame_digest()` 现已存在，联机时每 N 帧上报一次即可 |

## 6. UI（联机大厅 + 聊天浮层）

> 2026-11-04 起与 Web 端对齐：连接弹窗、房间列表、房间内、暂停/重连浮层拆成四份 UI 数据，
> 聊天从页面里拆出来变成独立浮层。

### 6.1 页面与层级

| 数据 | 层级 | 打开方式 |
|---|---|---|
| `ui/network_modal.ui.xml`（id `network_modal`） | layer 0 **弹窗** | 主页「联机对战」广播 `network_game` |
| `ui/network_rooms_page.ui.xml`（id `network_rooms_page`） | layer 0 页面 | 连接成功后 `set_page` |
| `ui/network_room_page.ui.xml`（id `network_room_page`） | layer 0 页面 | 进房后 `set_page` |
| `ui/network_overlay_page.ui.xml`（id `network_overlay_page`） | layer 1 页面 | 重连 / 重连失败 |
| `ui/chat_page.ui.xml`（id `chat`） | layer 2 浮层 | `T` 键 / 收到新消息的预览 |

这四份页面里都写着 `<component cls="NetworkLogic"/>`；`NetworkLogic` 是**单例组件**
（`SINGLETON = true`），实例挂在 `lfw.host_node` 上，页面打开时通过 `UINode::link_singleton`
回调 `on_link` 绑定各自的节点，关闭时 `on_unlink` 解绑。

### 6.2 已接的行为

- 连接：游客模式（地址 + 昵称），连接中禁用重复点击，断开/关闭回到入口。
- 房间列表：分页（每页 5 行）、自动刷新（5s）、创建、加入（带密码房要求密码）、刷新、断开。
- 房间内：同步模式（auto/lockstep/delay，房主可改）、改密码、准备/取消准备、踢人、离开；
  全员准备后 5s 倒计时，房主自动发 `RoomStart`。
- 聊天：`chat_store` 管内容与频道（房间内只能发房间频道、房间外只能发全局频道），
  `ChatLogic` 负责渲染与预览淡出（`PEEK_HOLD_MS` / `PEEK_FADE_MS`）。
- 重连：`Connection` 等价逻辑在 `host/net/net_client`，浮层显示尝试次数；失败可「继续单机」/「返回大厅」。

### 6.3 关键实现

```
lfw/net/chat_store.{h,cpp}           聊天数据 + 发送通道
lfw/net/chat_overlay.{h,cpp}         聊天层（layer 2）+ T 键热键
lfw/net/net_service.h                UI 使用的联机接口（INetService / INetListener）
lfw/ui/component/chat_logic.{h,cpp}  聊天浮层逻辑（换行/省略号、预览淡出）
lfw/ui/component/network_logic.{h,cpp} 四个页面的绑定与渲染
host/net/net_client.{h,cpp}          WebSocket 客户端：请求/响应、房间、Ping、重连、Tick 分发
host/godot/lfw_godot_ws.{h,cpp}      Godot `WebSocketPeer` 传输
```

### 6.4 与 Web 端的差异 / 待办

- **账号联机**：Web 端也还是占位（打开浏览器 + 模拟授权），native 目前只做游客模式。
- **`ref` 子节点**：`<node ref="builtin_data/launch/bg.ui.json"/>` 还没实现外部 UI 引用，
  联机页背景是空的（布局与交互不受影响）。
- **同步校验 / 数据包一致性**：`SyncChecker`、`Dataset` 广播、房间数据 MD5 比对尚未移植。
- **`KeyTick`**：与 Web 端一致，暂不使用。

### 6.5 验收

```bash
# 连接弹窗（真实数据包；`--zip` 指向 lf2s/origin/prel_data 才能看到新页面）
node tools/native.mjs godot-run -- --headless \
  --zip ../lf2s/origin/prel_data --zip build/host-data/data \
  --ui-page network_modal --frames 60 --ui-dump

# 房间列表 / 房间内 / 浮层 / 聊天
... --ui-page network_rooms_page --frames 60
... --ui-page network_room_page --frames 60
... --ui-page network_overlay_page --frames 60
... --ui-page chat --frames 60
```

## 6.6 帧同步接线（host 侧）

`LfwGodotHost` 里已有最小闭环：

- `RoomStart` → 选 `LockstepDriver` / `DelayDriver`（`host/net/net_sync`），
  设置 60UPS 与确定性参数、`mt.reset(seed)`、切到 `network_loading`。
- 每帧把本帧积累的 `lfw.cmds` 打包成 `Tick{seq: driver.submit_seq(), cmds[]}`
  （命令末尾带 `--from=<client_id>`），然后**清空本地命令**。
- 服务器回 `Tick{seq, reqs[]}` 后按数组顺序把各端命令压回 `lfw.cmds`，再推进一帧；
  `can_step()` 在有联机对局时改由帧驱动放行。

还没做：`Dataset` 同步、重连回放帧的驱动内重放（`Rejoin.resps` 目前只通知 UI）、
`KeyTick`、房间数据一致性校验。

## 7. 第一版 C++ 联机的最小集

**做**：`ClientInfo` / `CreateRoom` / `JoinRoom` / `ClientReady` / `RoomStart` / `Tick` /
`RoomSync` / `RoomContinue` / `Abandon` / `Ping` / `ListRooms`（简版）/ `Rejoin`+`RejoinFrames`（单包）。

**延后**：`KeyTick`（Web 端也没用）、聊天/管理员、`Dataset` 全量广播、分批回放。

**必须比 Web 端改进的地方**（Web 版的具体缺陷，别照抄）：

1. 输入**没有重传/超时**：任一客户端停发即全员永久卡死。C++：服务端对缺帧启 250–500ms 超时，超时把该客户端标 absent 并允许房主继续。
2. lockstep 客户端只留**一个**待处理 Tick（`_resp` 被覆盖）。C++：用 `std::map<seq,…>` 队列（`net_frame_driver` 已是）。
3. `send_nowait` **无背压**：每帧一条 JSON，高 RTT 时会堆积。C++：合并发送 + 服务端 `MAX_PIPELINE=16` 显式拒绝。
4. 校验串每帧拼长字符串 + md5，线上开销大；C++ 改为**每 N 帧一次 CRC32 摘要**，并保留最近若干帧用于定位分叉点。
5. `Team` 不由网络层决定：若要做到「按座位固定红蓝」，`RoomStart` 载荷里要加 `seat→team` 映射（向后兼容的新字段）。

## 8. 里程碑

| 里程碑 | 内容 | 验收 |
|---|---|---|
| M0（本次） | 协议/策略/帧驱动地基 + 自检 | `--net-selftest` PASS；60 帧 digest 不变 |
| M1 | WebSocket 传输 + 本地开一局（复用 `server/`） | 两实例进同一房间，60UPS 跑满 600 帧，命令流一致 |
| M2 | 种子注入 + tick 屏障 + 远程输入注入 | 两端 `frame_digest` 逐 60 帧一致 |
| M3 | delay 模式 + RTT 选型 | 人工加 80ms 延迟仍不卡顿、desync=0 |
| M4 | 掉线暂停 / `RoomContinue` / AI 补位 | 拔网线 → 全员暂停 → 继续 → 该座位转 AI |
| M5 | `Rejoin` / `RejoinFrames` 追平 | 断线 30s 内重连不中断对局 |
| M6 | Steam：大厅 + P2P（`ISteamNetworkingSockets`） | 无公网 IP 直连成功，`GetDetailedConnectionStatus` 可读 |

## 9. 排错

- **卡住不动**：先在服务端看「哪一帧缺谁」；Web 版没有超时，多半是有人停发了。
- **分叉（desync）**：比对两端同一 `seq` 的 `frame_digest`，再从命令流找出第一条不同的 `cmds`；
  常见原因是非确定来源（墙钟、未播种 RNG、容器遍历顺序、浮点/多线程）。
- **越玩越卡**：检查是否每帧都发了未压缩的长字符串（校验串），以及服务端 seq 窗口是否失控。
