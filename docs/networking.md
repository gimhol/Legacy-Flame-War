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

#### 6.3.1 请求体（对齐 Web 端）

| 消息 | 必带字段 | 说明 |
| --- | --- | --- |
| `ClientInfo` | `name`、`players` | 连接后先报名字，服务端给 `id` + `secret` |
| `CreateRoom` | `min_players`、`max_players`、`lfw_version`、`data_infos` | 默认 1~4 人；版本与数据包指纹由服务端记在房间上 |
| `JoinRoom` | `roomid`、`pwd?`、`lfw_version`、`data_infos` | `roomid` 是**字符串**（服务端 `'' + (++room_id)`），要原样回传；版本/数据包 MD5 不一致会被拒 |
| `Rejoin` / `Abandon` | `roomid`、`client_id`、`secret` | `roomid` 同样原样回传 |

`lfw_version` 取 `LFW::VERSION_NAME()`，`data_infos` 取 `LFW::collect_data_infos()`（只用到 `md5`）。

#### 6.3.2 页面绑定与生命周期

`NetworkLogic` 是单例组件，四个页面共享同一份绑定。**页面被销毁时（`on_unlink`）
必须把该页面上的所有节点指针置空**——否则下一次 `render_room()` / `update_topbar()`
就会写到已经析构的 `UINode` 上（表现为建房后别人加入时进程直接崩）。

`network_top_sub` / `network_account_chip` 这类节点**房间列表页和房间页里都有**，
早期实现按页面 id 各清各的，结果「房间页」那一份没清：开局后房间页被换掉，`account_chip`
仍指向已析构的节点，`update_topbar()` 每次 ping 都把昵称写进那块内存；内存随后被新页面
复用，于是**准备页的角色选择格里会冒出一个别人的昵称**（纯联机现象，位置随机）。

现在统一走 `forget_page_nodes()`：`on_unlink` 时按「节点属于哪个页面」（`node.root()`）
把所有成员指针清掉，不依赖任何"记得清哪个字段"的清单；所有写操作再过一层 `owned()`，
节点必须还活着、且属于本组件当前管着的页面（`page_roots`）。这样即使某个指针漏清，
也不会写到别人的节点上。

同理，`render_rooms()` / `render_room()` 分别要求 `rooms_page` / `room_page` 非空：
进房后房间列表页已经销毁，房间内的 `RoomSync` 更新不该去碰它。

#### 6.3.3 文本输入框（`TextInput`）

Web 端是真的 `<input>`；Godot 侧是覆盖在上面的 `LineEdit`。它必须挂在**自己的
`CanvasLayer`**（`GodotTextInputs::ensure_layer`，layer 99，控制台是 100）上：

- 游戏画面的宿主节点设了 `clip`（黑边裁切），子节点会被一起裁掉；
- 离屏 UI 是用自定义渲染器画的，`z_index >= 1`，直接当子节点会被 UI 盖住。

踩过的坑：覆盖层挂在宿主节点上时，联机弹窗里「地址 / 昵称」看着是空框、打字没反应
（字被画在 UI 底下），实际组件里是有值的。


### 6.4 与 Web 端的差异 / 待办

按「为什么和 `NetworkLogic.ts` 长得不一样」分三类：

1. **平台强制**（不算差异，但代码形态必然不同）：TS 里节点是对象引用 + `undefined`，
   页面销毁后残留的引用只是"写它没人看得见"；C++ 是裸指针，同样的残留就是悬垂写。
   所以 native 多出 `page_roots` / `owned()` / `forget_page_nodes()` 这一层——它顶替的是
   JS 的 GC，不是新行为。剩下的 `q(node, id, ptr)`、`bind_click()` 也只是 TS `q()`/`bind()` 的直译。
2. **还没做的功能**（不是偏差，是排期）：账号登录 UI（TS 15 个 `account_*` 字段）、
   hover/按下样式（TS `paint`/`paint_style`）、文字折行省略（TS `fit`/`fit_lines`）、
   语言切换时重建文字（TS `refresh_texts`/`apply_placeholders`）、
   以及里程碑 **M4 的掉线暂停**（TS `on_leaver` → `driver.suspend()`、`RoomContinue`、AI 补位）。
   TS 的 82 个节点字段在 native 有 57 个，缺的 25 个全在上面这两块里（15 个账号 + 8 个纯视觉容器 + 2 个 tab）。
3. **移植抄漏**（这些是 bug）：`unbind_room_page` 少清了 `top_sub` / `account_chip_bg` /
   `account_chip` / `btn_logout`（见 6.3.2 的昵称串格）；`update_overlay` 从没 push 过
   `network_overlay_page`（重连浮层永远不出现，已按 TS 补上 `sync_overlay_page`）；
   读秒结束的文案少了 `network.enter_game`。这三处已对齐。

其它已知差异：

- **昵称**：游客昵称按语言随机生成（`pick_default_nickname`），并裁到 10 个码点
  （对齐 Web 的 `NICKNAME_MAX_LENGTH`）；还没接本地存档（Web 端存在 localforage）。
- **`ref` 子节点**：`<node ref="builtin_data/launch/bg.ui.json"/>` 还没实现外部 UI 引用，
  联机页背景是空的（布局与交互不受影响）。
- **数据包一致性**：建房/加房已经把 `lfw_version` + `data_infos[].md5` 发给服务端（不合会被拒），
  但开局前的 `SyncChecker` / `Dataset` 广播仍未移植。
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

`LfwGodotHost` 里已是一套能真开局的闭环：

- `RoomStart` → 选 `LockstepDriver` / `DelayDriver`（`host/net/net_sync`），
  设置 60UPS 与确定性参数、`mt.reset(seed)`、`purge_entities()` + `reset_new_id/team`、
  建 `client#座位` 玩家槽位、切到 `network_loading`。
- `RoomStart` 之后**立刻补发 `[0, lead)` 的 `Tick`**（`net_prime_ticks`）。
  服务端要「所有在线客户端都提交同一 seq」才广播，谁都不发第 0 帧就永远没有帧，
  Web 端是 `NetworkLogic.lfw_cb.on_loading_end` 发的那一条。
- 首帧（`seq == 0`）到达 → `net_start_match()`：`reset_game_time`、`change_bg(RANDOM_BG)`、
  按座位生成角色、`set_page(vs_mode_page)`。
- 稳态每帧顺序固定为 **`driver.take()` → `net_submit_local_cmds()` → 压远端命令 → `update_once()`**：
  `take()` 会把 `_seq` 推进一帧，所以 `submit_seq() = _seq + lead - 1` 正好等于 TS 的 `seq + lead`；
  提交必须发生在 `step` 之前（`update_once` 末尾会 `clear_cmds()`）。

确定性约定（两端必须算出同一份世界）：

| 项 | 来源 |
|---|---|
| 背景 | `change_bg(RANDOM_BG)` 用 `mt`（已 `mt.reset(RoomStart.seed)`）挑，两端挑到同一个 |
| 角色数据 | `datas.fighters()[slot_index % size]`，`slot_index` 按 `reqs` 的服务端客户端顺序累加 |
| 分队 / 朝向 | 座位奇偶：`1P/3P → team 1 朝右`，`2P/4P → team 2 朝左` |
| 落位 | 纯几何（`bg.left/right`、屏幕宽、地面 `Ground::y`），不吃 RNG |
| 实体 id | `purge_entities()` 后再 `reset_new_id()`（计数器回到 100），两端从同一段 id 起算 |

还没做：`Dataset` 同步、重连回放帧的驱动内重放（`Rejoin.resps` 目前只通知 UI）、
`KeyTick`、房间数据一致性校验。

**和 Web 端的差异**：TS 的 `LFWNetworkDriver::start()` 只把页面切回 `main_page`、
不生成角色（Web 端联机对局本体还没做）；native 这边首帧直接进 `vs_mode_page` 并对局。
单人房间里没有对手，VS 规则会很快判胜出总结页——那是正常的，双人即为两队对局。

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
- **联机页打不开**：十有八九是数据包太旧（`network_modal` 等页面不在包里）。
  `tools/native.mjs` 会按 mtime 在 `build/host-data/prel`、`dist/prel.zip`、`lf2s/origin/prel_data`
  之间挑最新的一个；`UILayer` 找不到页面 id 会打 `[UILayer] ui not found`。
- **输入框打不了字 / 看着是空框**：确认 `LineEdit` 覆盖层是不是又挂回宿主节点了（见 6.3.3）。
- **房间列表里看不到刚建的房 / 加房报 `room not found`**：房间随「房主连接」存活，
  建房进程一退出房间就没了；另外 `JoinRoom` 的 `roomid` 必须是房间自己的 id（原样回传）。
- **`ERROR: Invalid protocol: "https://"`**：传输层是 WebSocket，只认 `ws://`/`wss://`。
  `NetClient::open` 现在会把 `http(s)://` 和裸主机统一规整成 `wss://` + `ws://` 两个候选，
  不会再把这个 URL 丢给 `WebSocketPeer`。
- **`ERROR: TLS handshake error: -9984` / `Invalid status code. Got: '307', expected '101'`**：
  服务器侧问题，不是客户端。前者是 `wss` 的证书不被信任（过期/域名不符），
  后者是 `ws` 被反代强制跳转到 https。用 `openssl`/浏览器看下证书有效期，
  证书过期时客户端无论怎么重试都连不上（**不要**去关 TLS 校验来绕过）。
  连不上时弹窗会显示「连接失败」（`network.connect_failed`）。
- **`[godot] ws: closed code=...`**：传输层被对端关掉了；`code=1006` 通常是进程被强杀
  （验收脚本跑完 `--frames` 直接结束）而不是协议错误。
- **倒计时归零瞬间崩溃（signal 11，栈里最深的一层是 `GodotTextInputs::sync`）**：
  输入框覆盖层用的是**上一帧渲染时收集的 `UINode*`/`TextInput*`**。`RoomStart` 在
  `_process` 开头（`net_poll`）就处理，会把联机页面 `set_page` 掉、节点当场析构；接着这一帧
  若渲染被跳过（`Sync`/`Half`/目标帧率模式都可能跳），`sync` 就会拿**已释放**的指针去读
  `input->text()` / `node->focused()` → 段错误。现在 `UILayers` 带页面栈代数
  （`generation()`），渲染时把它记进 `GodotWorldRenderer::ui_inputs_generation()`；
  `sync_text_input` 发现代数对不上就整批丢弃覆盖层，等下一次渲染重建。
- **准备页（角色选择框）里冒出一个陌生昵称 / 玩家名字段是随机位置的一串名字**：联机 UI 是单例，
  页面销毁后它还在跑；`network_top_sub`、`network_account_chip` 这些**两个页面共用的节点**以前
  只按页面 id 清一份，房间页被换掉后还留着野指针，`update_topbar()` 每次 ping 都把昵称写进
  已释放的 `UINode`，那块内存又被新页面的格子复用 → 昵称"长"在角色选择格里。现在
  `on_unlink` 统一 `forget_page_nodes()`（按 `node.root()` 归属清），并给所有写操作加了
  `owned()` 守卫（见 6.3.2）。
- **`--window-resize WxH[@帧]`**：验收脚本里按帧改窗口像素尺寸（复现缩放相关的绘制问题用）。
- **回到主页后点菜单要等一帧才生效 / 快速连点两次会落到新页面上**：这是**与 Web 端一致**
  的行为，不是 bug。`RoomStart` 之后帧驱动一直活着（TS 也一样，`LFWNetworkDriver::start()`
  不释放 driver），于是 `push_frame` 会把本地 `lfw.cmds`（含所有 UI 命令）清空并打包
  `Tick` 发给服务器，本地要等这一帧被回显后才执行 —— 延迟正好一个 RTT。
  连接真的断开时才释放：`net_poll()` 检测到 `!opened() && !connecting() && !reconnecting()`
  就调 `net_release_world()`，对应 Web 端 `NetworkLogic::release_world()`（清钩子、
  `world.awake()`、`driver = null`）。少了这一步，退出房间后 `can_step()` 永远等不到 Tick，
  整个界面会冻死。
- **倒计时结束仍停在加载页 / 世界和 UI 一起冻住**：先看有没有第 0 帧。服务端要收齐所有
  在线客户端的同一 `seq` 才广播，所以 `RoomStart` 后必须由客户端补发 `Tick{seq:0}`
  （`net_prime_ticks`）；再看 `push_frame` 的提交顺序是不是 `take()` 之后才
  `net_submit_local_cmds()`（提交早了会差一帧，首帧后再也提交不上，`can_step()` 永远 false）。
- **进了对局但看不到自己的角色**：`reset_new_id()` 把计数器打回 100，如果世界里还有旧实体
  占着 101/102，新角色会撞号并被 `World::add_entities` 静默丢掉（日志里没有报错，只是
  `world.entities` 不增长）。开局前必须 `world.purge_entities()`。
- **房间里的按钮点不动、聊天预览浮层一出现就点不中「准备」**：指针命中规则是「最上层只要
  有命中就独占输入」，`chat` 浮层是全屏节点，所以预览态必须 `set_touchable(false)`
  （见 `ChatLogic::on_start`）。
