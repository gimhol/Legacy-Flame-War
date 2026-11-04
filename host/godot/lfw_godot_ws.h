#pragma once

#include <godot_cpp/classes/web_socket_peer.hpp>

#include <string>
#include <vector>

#include "host/net/net_transport.h"

namespace lfw::host {

/** WebSocket 传输：用 Godot 的 `WebSocketPeer` 连 Web 端已有的联机服务器。 */
class GodotWsTransport final : public net::INetTransport {
 public:
  GodotWsTransport() = default;

  void connect(const std::string& url) override;
  void poll() override;
  bool drain(std::vector<std::string>& out) override;
  void send(const std::string& text) override;
  bool connected() const override;
  void close();

 private:
  godot::Ref<godot::WebSocketPeer> _peer;
  bool _requested = false;
  bool _open = false;
};

}  // namespace lfw::host
