#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "host/net/net_transport.h"

namespace lfw::host::net {

/**
 * 进程内回环：按固定 RTT 延迟投递文本，时间由测试自己推进（不读墙钟，结果可复现）。
 * 既当"客户端 × N"，也提供“服务端”收件箱，用来在没有真服务器的情况下验证锁步调度。
 */
class LoopbackHub {
 public:
  explicit LoopbackHub(double rtt_ms) : _rtt_ms(rtt_ms > 0.0 ? rtt_ms : 0.0) {}

  int attach();
  int endpoint_count() const { return static_cast<int>(_clients.size()); }

  void advance(double ms) { _now += ms > 0.0 ? ms : 0.0; }

  void send_from(int index, const std::string& text);
  void send_to(int index, const std::string& text);

  std::vector<std::pair<int, std::string>> drain_server();
  std::vector<std::string> drain_client(int index);

  double now_ms() const { return _now; }
  double rtt_ms() const { return _rtt_ms; }

 private:
  struct Packet {
    double at = 0.0;
    int to = -1;
    int from = -1;
    std::string text;
  };

  void post(int to, int from, const std::string& text);
  void flush();

  double _rtt_ms = 0.0;
  double _now = 0.0;
  std::vector<std::vector<std::string>> _clients;
  std::vector<std::pair<int, std::string>> _server_inbox;
  std::vector<Packet> _inflight;
};

class LoopbackTransport final : public INetTransport {
 public:
  LoopbackTransport(LoopbackHub& hub, int index) : _hub(hub), _index(index) {}

  void connect(const std::string& url) override;
  void poll() override;
  bool drain(std::vector<std::string>& out) override;
  void send(const std::string& text) override;
  bool connected() const override { return _connected; }
  int rtt_ms() const override { return static_cast<int>(_rtt_ms); }

  int index() const { return _index; }

 private:
  LoopbackHub& _hub;
  int _index = -1;
  bool _connected = false;
  int _rtt_ms = 0;
};

}
