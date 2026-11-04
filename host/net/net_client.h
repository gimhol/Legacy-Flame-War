#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "host/net/net_transport.h"
#include "lfw/core/value.h"
#include "lfw/net/net_service.h"

namespace lfw::host::net {

/**
 * 联机客户端：Web 端 `Connection` 的 C++ 版（请求/响应、房间状态、Ping、重连）。
 *
 * 只认识传输抽象 `INetTransport`（Godot 里是 `WebSocketPeer`），
 * 对外暴露 `lfw::net::INetService` 给 UI 用。
 */
class NetClient final : public lfw::net::INetService {
 public:
  explicit NetClient(std::unique_ptr<INetTransport> transport);
  ~NetClient() override;

  void add_listener(lfw::net::INetListener* listener) override;
  void remove_listener(lfw::net::INetListener* listener) override;

  bool opened() const override;
  bool connecting() const override { return _connecting; }
  bool reconnecting() const override { return _reconnecting; }
  double rtt() const override { return _rtt; }
  std::u16string url() const override;

  const std::optional<lfw::net::NetClientInfo>& client() const override { return _client; }
  const std::optional<lfw::net::NetRoomInfo>& room() const override { return _room; }
  const std::vector<lfw::net::NetRoomInfo>& rooms() const override { return _rooms; }

  std::u16string nickname() const override { return _nickname; }
  void set_nickname(const std::u16string& nickname) override;
  void set_players(const std::vector<std::u16string>& players) override;

  void open(const std::u16string& url) override;
  void close() override;
  void give_up() override;

  void send_nowait(const std::u16string& type, const Value& payload) override;
  void request(const std::u16string& type, const Value& payload, double timeout_ms,
               lfw::net::NetOk ok, lfw::net::NetFail fail) override;

  void enable_rejoin() override { _rejoin_enabled = true; }
  void disable_rejoin() override { _rejoin_enabled = false; }

  int rejoin_seq() const override { return _rejoin_next_seq; }
  void set_rejoin_seq(int seq) { _rejoin_next_seq = seq; }
  void set_frame_sink(std::function<void(const Value& resp)> sink) override {
    _frame_sink = std::move(sink);
  }
  /** 收到 `RoomStart`（开局参数）时回调。 */
  void set_room_start_sink(std::function<void(const Value& resp)> sink) {
    _room_start_sink = std::move(sink);
  }
  /** 连接过程日志（宿主接到自己的 log 里，便于排查「连不上 / 打不开」）。 */
  void set_logger(std::function<void(const std::string&)> logger) { _log = std::move(logger); }
  void submit_local_cmds(const std::vector<std::u16string>& cmds) override { (void)cmds; }

  /** 每帧推进：收发、超时、Ping、重连。 */
  void poll();

 private:
  struct Job {
    double deadline = 0.0;
    bool has_deadline = false;
    lfw::net::NetOk ok;
    lfw::net::NetFail fail;
  };

  bool transport_open() const;
  double now_ms() const;
  void handle_open();
  void handle_close(bool intentional);
  void handle_text(const std::string& utf8);
  void handle_resp(const Value& resp);
  void update_room_from_resp(const Value& resp);
  void emit_room();
  void emit_rooms();
  void submit_client();
  void try_next_url();
  void schedule_reconnect();
  void cancel_reconnect();
  void submit_rejoin();
  void pull_rejoin_frames(const Value& first, size_t index, std::vector<Value> acc);
  void send_ping();
  void fail_all_jobs();
  std::string next_pid();
  std::string send_req(const std::u16string& type, const Value& payload);
  void notify_error(const std::u16string& text);

  std::unique_ptr<INetTransport> _transport;
  std::vector<lfw::net::INetListener*> _listeners;

  double _pid = 1.0;
  std::optional<lfw::net::NetClientInfo> _client;
  std::optional<lfw::net::NetRoomInfo> _room;
  std::vector<lfw::net::NetRoomInfo> _rooms;
  std::u16string _nickname;
  std::vector<std::u16string> _players;
  std::u16string _secret;
  std::string _last_url;
  std::vector<std::string> _urls;

  bool _connecting = false;
  bool _ever_open = false;
  bool _reconnecting = false;
  bool _rejoin_enabled = false;
  bool _dead = false;
  int _reconnect_attempt = 0;
  double _reconnect_at = 0.0;
  double _connect_deadline = 0.0;
  double _next_ping = 0.0;
  double _rtt = 0.0;
  int _rejoin_next_seq = 0;
  std::string _resume_client_id;

  std::map<std::string, Job> _jobs;
  std::function<void(const Value& resp)> _frame_sink;
  std::function<void(const Value& resp)> _room_start_sink;
  std::function<void(const std::string&)> _log;
  std::vector<std::string> _inbox;
};

}
