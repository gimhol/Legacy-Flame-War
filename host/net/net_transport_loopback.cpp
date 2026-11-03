#include "host/net/net_transport_loopback.h"

#include <algorithm>

namespace lfw::host::net {

int LoopbackHub::attach() {
  _clients.emplace_back();
  return static_cast<int>(_clients.size()) - 1;
}

void LoopbackHub::post(int to, int from, const std::string& text) {
  _inflight.push_back(Packet{_now + _rtt_ms * 0.5, to, from, text});
}

void LoopbackHub::flush() {
  std::vector<Packet> rest;
  rest.reserve(_inflight.size());
  for (Packet& p : _inflight) {
    if (p.at > _now) {
      rest.push_back(std::move(p));
      continue;
    }
    if (p.to < 0) {
      _server_inbox.emplace_back(p.from, p.text);
    } else if (p.to < static_cast<int>(_clients.size())) {
      _clients[static_cast<std::size_t>(p.to)].push_back(p.text);
    }
  }
  _inflight = std::move(rest);
}

void LoopbackHub::send_from(int index, const std::string& text) {
  if (index < 0 || index >= static_cast<int>(_clients.size())) return;
  flush();
  post(-1, index, text);
}

void LoopbackHub::send_to(int index, const std::string& text) {
  if (index < 0 || index >= static_cast<int>(_clients.size())) return;
  flush();
  post(index, -1, text);
}

std::vector<std::pair<int, std::string>> LoopbackHub::drain_server() {
  flush();
  std::vector<std::pair<int, std::string>> out;
  out.swap(_server_inbox);
  return out;
}

std::vector<std::string> LoopbackHub::drain_client(int index) {
  flush();
  if (index < 0 || index >= static_cast<int>(_clients.size())) return {};
  std::vector<std::string> out;
  out.swap(_clients[static_cast<std::size_t>(index)]);
  return out;
}

void LoopbackTransport::connect(const std::string& url) {
  (void)url;
  _connected = true;
  _rtt_ms = static_cast<int>(_hub.rtt_ms());
}

void LoopbackTransport::poll() {}

bool LoopbackTransport::drain(std::vector<std::string>& out) {
  std::vector<std::string> got = _hub.drain_client(_index);
  if (got.empty()) return false;
  out.insert(out.end(), got.begin(), got.end());
  return true;
}

void LoopbackTransport::send(const std::string& text) { _hub.send_from(_index, text); }

}
