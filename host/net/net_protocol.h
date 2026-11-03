#pragma once

#include <optional>
#include <string>
#include <vector>

#include "host/net/net_sync.h"
#include "lfw/core/value.h"

namespace lfw::host::net {

/** 与 Web 端 `src/Net/MsgEnum.ts` 对齐的消息类型（第一版只用到这里的子集） */
enum class MsgType {
  kUnknown,
  kClientInfo,
  kCreateRoom,
  kJoinRoom,
  kExitRoom,
  kClientReady,
  kRoomStart,
  kRoomSync,
  kListRooms,
  kTick,
  kPing,
  kError,
};

struct Msg {
  bool is_req = true;
  std::string pid;
  MsgType type = MsgType::kUnknown;
  Value payload = Value();
};

/** 一个客户端在某一帧提交的输入（`cmds` 是 LFW 命令字符串，含 `--from=<client_id>`） */
struct ClientTick {
  std::string client_id;
  int seq = 0;
  std::vector<std::string> cmds;
  std::string checksum;
};

/** 服务端收齐后广播的整帧输入；`reqs` 的顺序就是各端执行命令的顺序 */
struct FrameInputs {
  int seq = 0;
  std::vector<ClientTick> reqs;
};

struct RoomStartInfo {
  double seed = 0.0;
  SyncPlan plan;
};

const char* msg_type_name(MsgType type);
MsgType parse_msg_type(const std::u16string& name);

/** 信封编解码：`{is_req, pid, type, ...payload}` ↔ utf8 JSON 文本 */
std::string encode_envelope(const Msg& msg);
std::optional<Msg> decode_envelope(const std::string& utf8);

Value make_tick_req(int seq, const std::vector<std::string>& cmds, const std::string& checksum);
std::optional<ClientTick> parse_tick_req(const Msg& msg);

Value make_tick_resp(int seq, const std::vector<ClientTick>& reqs);
std::optional<FrameInputs> parse_tick_resp(const Msg& msg);

Value make_room_start_resp(double seed, const SyncPlan& plan);
std::optional<RoomStartInfo> parse_room_start(const Msg& msg);

}
