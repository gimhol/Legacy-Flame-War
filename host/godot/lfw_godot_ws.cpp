#include "host/godot/lfw_godot_ws.h"

#include <godot_cpp/classes/web_socket_peer.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <memory>

namespace lfw::host {

namespace {

godot::Ref<godot::WebSocketPeer> make_peer() {
  godot::Ref<godot::WebSocketPeer> peer;
  peer.instantiate();
  return peer;
}

}  // namespace

void GodotWsTransport::connect(const std::string& url) {
  if (_peer.is_valid()) {
    _peer->close();
    _peer.unref();
  }
  _open = false;
  _requested = !url.empty();
  if (!_requested) return;
  _peer = make_peer();
  _peer->connect_to_url(godot::String::utf8(url.c_str()));
}

void GodotWsTransport::poll() {
  if (!_peer.is_valid()) return;
  _peer->poll();
  _open = _peer->get_ready_state() == godot::WebSocketPeer::STATE_OPEN;
}

bool GodotWsTransport::drain(std::vector<std::string>& out) {
  if (!_peer.is_valid()) return false;
  bool any = false;
  while (_peer->get_available_packet_count() > 0) {
    const godot::PackedByteArray packet = _peer->get_packet();
    const int size = packet.size();
    std::string text;
    text.resize(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) text[static_cast<size_t>(i)] = static_cast<char>(packet[i]);
    out.push_back(std::move(text));
    any = true;
  }
  return any;
}

void GodotWsTransport::send(const std::string& text) {
  if (!_peer.is_valid() || !_open) return;
  _peer->send_text(godot::String::utf8(text.c_str()));
}

bool GodotWsTransport::connected() const { return _open; }

void GodotWsTransport::close() { connect(std::string()); }

}  // namespace lfw::host
