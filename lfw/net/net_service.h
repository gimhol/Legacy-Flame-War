#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace net {

/** 一个客户端（玩家组）的基础信息。 */
struct NetClientInfo {
  std::u16string id;
  std::u16string name;
  /** 该客户端各座位（1P~4P）的玩家名，对齐 Web 端 `IClientInfo.players`。 */
  std::vector<std::u16string> players;
};

/** 房间内的一个客户端。 */
struct NetRoomClient {
  NetClientInfo info;
  bool ready = false;
};

/** 数据包元信息（房间数据一致性校验用）。 */
struct NetDataInfo {
  std::u16string type;
  std::u16string title;
  double version = 0.0;
  std::u16string md5;
};

/** 房间信息（对齐 Web 端 `IRoomInfo`）。 */
struct NetRoomInfo {
  /** 房间 id：服务端发的是**数字**，`JoinRoom`/`Rejoin`/`Abandon` 都要原样回传（强等比较）。 */
  Value id;
  std::u16string title;
  std::u16string code;
  NetRoomClient owner;
  std::vector<NetRoomClient> clients;
  int min_players = 0;
  int max_players = 8;
  bool started = false;
  bool need_pwd = false;
  /** `auto` / `lockstep` / `delay`。 */
  std::u16string sync_mode = u"auto";
  std::u16string lfw_version;
  std::vector<NetDataInfo> data_infos;
};

/** 联机事件回调（UI 只关心这些）。 */
class INetListener {
 public:
  virtual ~INetListener() = default;
  virtual void on_net_open() {}
  virtual void on_net_close(const std::u16string& reason) { (void)reason; }
  virtual void on_net_room(const NetRoomInfo* room) { (void)room; }
  virtual void on_net_rooms(const std::vector<NetRoomInfo>& rooms) { (void)rooms; }
  virtual void on_net_ping(const std::u16string& client_id, double rtt) {
    (void)client_id;
    (void)rtt;
  }
  virtual void on_net_reconnecting(int attempt) { (void)attempt; }
  virtual void on_net_rejoin(const Value& resp) { (void)resp; }
  virtual void on_net_rejoin_failed(bool rejected) { (void)rejected; }
  virtual void on_net_error(const std::u16string& text) { (void)text; }
  /** 服务器推来的原始响应（`type` 见 `MsgEnum`）。 */
  virtual void on_net_message(const Value& resp) { (void)resp; }
};

using NetOk = std::function<void(const Value& resp)>;
using NetFail = std::function<void(const std::u16string& error)>;

/**
 * 联机服务：Web 端 `Connection` + `LFWNetworkDriver` 的宿主侧实现，
 * 由宿主（Godot / headless）创建，UI 层只通过这个接口使用。
 */
class INetService {
 public:
  virtual ~INetService() = default;

  virtual void add_listener(INetListener* listener) = 0;
  virtual void remove_listener(INetListener* listener) = 0;

  virtual bool opened() const = 0;
  virtual bool connecting() const = 0;
  virtual bool reconnecting() const = 0;
  virtual double rtt() const = 0;
  virtual std::u16string url() const = 0;

  virtual const std::optional<NetClientInfo>& client() const = 0;
  virtual const std::optional<NetRoomInfo>& room() const = 0;
  virtual const std::vector<NetRoomInfo>& rooms() const = 0;

  virtual std::u16string nickname() const = 0;
  virtual void set_nickname(const std::u16string& nickname) = 0;
  virtual void set_players(const std::vector<std::u16string>& players) = 0;

  virtual void open(const std::u16string& url) = 0;
  virtual void close() = 0;
  /** 放弃重连并主动退房（对齐 `Connection.give_up`）。 */
  virtual void give_up() = 0;

  /** 不需要回包的请求（每帧 Tick 用）。 */
  virtual void send_nowait(const std::u16string& type, const Value& payload) = 0;
  /** 需要回包的请求：`ok` / `fail` 二选一，超时走 `fail`。 */
  virtual void request(const std::u16string& type, const Value& payload, double timeout_ms,
                       NetOk ok, NetFail fail) = 0;

  virtual void enable_rejoin() = 0;
  virtual void disable_rejoin() = 0;

  /** 已执行的最后一帧序号 + 1（重连回放起点）。 */
  virtual int rejoin_seq() const = 0;
  /** 服务器推来的帧输入 / 回放帧交给宿主推进世界。 */
  virtual void set_frame_sink(std::function<void(const Value& resp)> sink) = 0;
  /** 本地输入（命令字符串）交给网络层按帧提交。 */
  virtual void submit_local_cmds(const std::vector<std::u16string>& cmds) = 0;
};

}  // namespace net
}  // namespace lfw
