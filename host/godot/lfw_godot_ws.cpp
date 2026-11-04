#include "host/godot/lfw_godot_ws.h"

#include <godot_cpp/classes/web_socket_peer.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <memory>

#include <godot_cpp/variant/utility_functions.hpp>

namespace lfw::host {

namespace {

godot::Ref<godot::WebSocketPeer> make_peer() {
  godot::Ref<godot::WebSocketPeer> peer;
  peer.instantiate();
  return peer;
}

void ws_log(const std::string& text) {
  godot::UtilityFunctions::print(godot::String::utf8(text.c_str()));
}

}  // namespace

void GodotWsTransport::connect(const std::string& url) {
  if (_peer.is_valid()) {
    _peer->close();
    _peer.unref();
  }
  _open = false;
  _failed = false;
  _requested = !url.empty();
  if (!_requested) return;
  _peer = make_peer();
  const godot::Error err = _peer->connect_to_url(godot::String::utf8(url.c_str()));
  if (err != godot::OK) {
    // 传输层只认 ws:// wss://；这里当场失败，让上层立刻换候选而不是干等超时。
    _failed = true;
    _requested = false;
    _peer.unref();
  }
}

void GodotWsTransport::poll() {
  if (!_peer.is_valid()) return;
  _peer->poll();
  const bool was_open = _open;
  _open = _peer->get_ready_state() == godot::WebSocketPeer::STATE_OPEN;
  if (was_open && !_open) {
    ws_log("[godot] ws: closed code=" + std::to_string(_peer->get_close_code()) +
                           " reason='" + std::string(_peer->get_close_reason().utf8().get_data()) +
                           "'");
  }
  // 还没连上就进了 CLOSED：TLS 握手失败 / DNS 解析失败 / 被拒绝，立刻标记失败，
  // 让上层马上换下一个候选（否则要白等满一个连接超时）。
  if (!_open && _requested && !was_open &&
      _peer->get_ready_state() == godot::WebSocketPeer::STATE_CLOSED) {
    _failed = true;
    _requested = false;
  }
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
  const godot::Error err = _peer->send_text(godot::String::utf8(text.c_str()));
  if (err != godot::OK) {
    ws_log("[godot] ws: send failed err=" + std::to_string(static_cast<int>(err)) +
                           " bytes=" + std::to_string(text.size()) + " head=" +
                           text.substr(0, 120));
  }
}

bool GodotWsTransport::connected() const { return _open; }

void GodotWsTransport::close() { connect(std::string()); }

}  // namespace lfw::host
