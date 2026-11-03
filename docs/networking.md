# 联机（netplay）

> 状态：**地基**（协议/同步策略/帧驱动 + 自检）。真正的传输（WebSocket、Steam P2P）与房间流程尚未接进游戏。
> 自检：`node tools/native.mjs godot-run -- --headless --net-selftest --frames 5`

## 1. 现状

| 层 | Web/TS | native（本仓库） |
|---|---|---|
| 协议 | `src/Net/**` 33 个文件，WS 文本帧 + JSON 信封 `{is_req,pid,type,…}` | 无 |
| 服务端 | `server/`（Node + ws）：房间、帧聚合、重连回放 | 无 |
| 同步策略 | `src/Net/net_sync.ts`：`recommend_sync` / `resolve_sync` | **已移植**：`host/net/net_sync.{h,cpp}` |
| 帧驱动 | `LockstepNetworkDriver` / `DelayNetworkDriver` | **已移植**：`host/net/net_frame_driver.{h,cpp}` |
| 传输 | `WebSocket` | 只有 `INetTransport` 抽象 + 回环实现（`host/net/net_transport*.{h,cpp}`） |
| 确定性底座 | 固定 60UPS + `mt.reset(seed)` | 固定步长与 digest 已有；**`_mt` 种子不可注入**、无 tick 屏障、无远程输入注入 |
| Steam | — | 只到 `init/poll/overlay`（`host/godot/lfw_godot_steam.{h,cpp}`），SDK 里 `ISteamNetworkingSockets` / `ISteamMatchmaking` 都在但没接 |

## 2. 已落地（本仓库）

```
host/net/
  net_sync.{h,cpp}              同步策略：NET_UPS=60, LOCKSTEP_RTT_RATIO=1.25,
                                RTT_JITTER_MARGIN=1.3, MIN_LEAD=2, MAX_LEAD=8, UNKNOWN_RTT=100
  net_protocol.{h,cpp}          信封 + Tick 请求/响应 + RoomStart 的 JSON 编解码（字段与 Web 对齐）
  net_transport.h               传输抽象（connect/poll/drain/send/connected/rtt_ms）
  net_transport_loopback.{h,cpp} 进程内回环 + 固定 RTT 投递（时间由测试推进，结果可复现）
  net_frame_driver.{h,cpp}      IFrameDriver / LockstepDriver(lead=1) / DelayDriver(lead=K)
  net_selftest.{h,cpp}          自检：策略表、协议往返、两种驱动的回环调度
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

## 6. UI（联机大厅）
页面数据本来就是现成的：`lf2s/origin/prel_data/ui/network_page.ui.xml`（22KB，id `network_page`），
native 的 XML UI 解析器能直接加载，节点 id 与 Web 端 `NetworkLogic.ts` 一一对应。
缺的只是**组件**，现已补上：

```
lfw/ui/component/network_logic.{h,cpp}   // 组件注册名 NetworkLogic
```

已接的行为（全部用桩数据，不联网）：

| 交互 | 结果 |
|---|---|
| 主页「联机对战」→ `push_page(network_page)` | 打开大厅，连接面板 + `未连接` |
| 地址 / 昵称输入框 | 点击聚焦（末尾显示 `\|` 光标），键盘输入 / 退格 / Enter 提交、Esc 取消；长度上限 64 / 16 |
| 连接 | 状态 `已连接（桩数据）`，切到房间列表，填 8 个桩房间（含带密码与已开局的） |
| 房间列表分页 | 每页 5 行，标题旁显示 `第 1/2 页 · 共 8 个`，上一页/下一页生效 |
| 房间行 | 名称 / `#代号` / `人数/8` / 徽标（`等待` 或 `游戏中`）；带锁房间显示「密码」与输入框 |
| 点房间行 | 未开局才进；带锁房间要求密码 `123`（错则提示并把焦点给密码框）；进入后座位表按人数生成（我 + 玩家N，含 rtt/准备态） |
| 准备 / 开始 / 离开 / 返回大厅 / 断开 | 状态与面板正确切换；准备状态同步到座位表 |
| 聊天 | 连接后出现聊天面板：点击输入框输入、Enter 或「发送」上屏，最近 15 条（行高 19），右上角「房间/世界」目标切换 |
| 页签 | 「游客联机 / 账号联机」切换 body；账号：登录 → 等待认证 → 模拟认证（`Gim` / `gim_1024`）→ 进入大厅 / 退出 |
| 创建房间 | 新建桩房间并直接进入 |

布局常量（与 Web 端一致）：房间行高 56、玩家行高 32、聊天行高 19；房间行子节点顺序
`name/code/badge_bg/lock_bg/pwd_bg/join/owner/meta`，玩家行 `name/tags/rtt/state/kick`，
聊天行是 `size=0,19` 的模板（代码里用 `set_w(268)` 撑开才会显示文字）。

**还没接**：真实传输（`INetTransport`）、`TextInput` 组件（现在是 `NetworkLogic` 自己建的光标/退格，
接入后应换成组件的焦点/IME 路径）、房主改密码/踢人、`Dataset` 广播。接上 `host/net` 后把 `DPtr` 里的桩数据换成会话回调即可。

验收方式：

```bash
# 初始态
node tools/native.mjs godot-run -- --headless --ui-page network_page --frames 60 --ui-dump

# 连接 → 进房 → 聊天（KEY_EVENT 是唯一会派发到 UI 层的按键注入路径，`--key` 只走游戏内输入）
node tools/native.mjs godot-run -- --headless --ui-page network_page --frames 260 \
  --click 357,321@25 --click 100,104@35 --click 620,410@45 \
  --console-exec "KEY_EVENT --p=0 --s=1 --c=h --n=h@55" \
  --console-exec "KEY_EVENT --p=0 --s=1 --c=i --n=i@61" \
  --console-exec "KEY_EVENT --p=0 --s=1 --c=enter --n=enter@67" --ui-dump

# 分页 + 带锁房间（密码 123）
  --click 297,402@40 --click 205,402@50 --click 100,200@60 ...

# 账号页签（连接前）
  --click 315,148@20 --click 332,321@30 --click 332,321@40 --ui-dump
```

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
