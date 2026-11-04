#include "host/net/net_client.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

#include "lfw/core/json.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"

namespace lfw::host::net {

namespace {

constexpr double kConnectTimeoutMs = 10000.0;
constexpr double kPingIntervalMs = 500.0;
constexpr double kReconnectBaseMs = 300.0;
constexpr double kReconnectMaxMs = 3000.0;
constexpr int kMaxReconnectAttempts = 30;

std::u16string to_u16(const std::string& utf8) {
  return decode_utf8(std::vector<uint8_t>(utf8.begin(), utf8.end()));
}

std::string to_u8(const std::u16string& text) {
  const std::vector<uint8_t> buf = encode_utf8(text);
  return std::string(buf.begin(), buf.end());
}

std::u16string text_of(const Value& holder, const char16_t* key) {
  const Value value = field_or(holder, key);
  const std::u16string* const s = std::get_if<std::u16string>(&value);
  return s != nullptr ? *s : std::u16string();
}

double num_of(const Value& holder, const char16_t* key, double fallback) {
  const double v = to_number(field_or(holder, key));
  return std::isnan(v) ? fallback : v;
}

bool bool_of(const Value& holder, const char16_t* key, bool fallback = false) {
  const Value v = field_or(holder, key);
  if (std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v)) {
    return fallback;
  }
  return truthy(v);
}

lfw::net::NetClientInfo parse_client(const Value& v) {
  lfw::net::NetClientInfo out;
  out.id = text_of(v, u"id");
  out.name = text_of(v, u"name");
  return out;
}

lfw::net::NetDataInfo parse_data_info(const Value& v) {
  lfw::net::NetDataInfo out;
  out.type = text_of(v, u"type");
  out.title = text_of(v, u"title");
  out.version = num_of(v, u"version", 0.0);
  out.md5 = text_of(v, u"md5");
  return out;
}

lfw::net::NetRoomInfo parse_room(const Value& v) {
  lfw::net::NetRoomInfo out;
  out.id = field_or(v, u"id");
  out.title = text_of(v, u"title");
  out.code = text_of(v, u"code");
  const Value owner_src = field_or(v, u"owner");
  if (as_object(owner_src) != nullptr) {
    out.owner.info = parse_client(owner_src);
    out.owner.ready = true;
  }
  if (const Array* const clients = as_array(field_or(v, u"clients"))) {
    for (const Value& item : clients->items()) {
      lfw::net::NetRoomClient client;
      client.info = parse_client(item);
      client.ready = bool_of(item, u"ready");
      out.clients.push_back(std::move(client));
    }
  }
  out.min_players = static_cast<int>(num_of(v, u"min_players", 0.0));
  out.max_players = static_cast<int>(num_of(v, u"max_players", 8.0));
  out.started = bool_of(v, u"started");
  out.need_pwd = bool_of(v, u"need_pwd");
  out.sync_mode = text_of(v, u"sync_mode");
  if (out.sync_mode.empty()) out.sync_mode = u"auto";
  out.lfw_version = text_of(v, u"lfw_version");
  if (const Array* const infos = as_array(field_or(v, u"data_infos"))) {
    for (const Value& item : infos->items()) out.data_infos.push_back(parse_data_info(item));
  }
  return out;
}

bool same_text(const std::u16string& a, const std::u16string& b) { return a == b; }

}  // namespace

NetClient::NetClient(std::unique_ptr<INetTransport> transport)
    : _transport(std::move(transport)) {}

NetClient::~NetClient() {
  if (_transport != nullptr) _transport->send(std::string());
}

void NetClient::add_listener(lfw::net::INetListener* listener) {
  if (listener == nullptr) return;
  if (std::find(_listeners.begin(), _listeners.end(), listener) == _listeners.end()) {
    _listeners.push_back(listener);
  }
}

void NetClient::remove_listener(lfw::net::INetListener* listener) {
  _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

bool NetClient::opened() const { return _transport != nullptr && _transport->connected(); }

std::u16string NetClient::url() const { return to_u16(_last_url); }

void NetClient::set_nickname(const std::u16string& nickname) {
  _nickname = nickname;
  if (opened()) submit_client();
}

void NetClient::set_players(const std::vector<std::u16string>& players) {
  _players = players;
  if (opened()) submit_client();
}

double NetClient::now_ms() const {
  using clock = std::chrono::steady_clock;
  static const clock::time_point start = clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
  return static_cast<double>(ms.count());
}

std::string NetClient::next_pid() {
  _pid += 1.0;
  return std::to_string(static_cast<long long>(_pid));
}

std::string NetClient::send_req(const std::u16string& type, const Value& payload) {
  const std::string pid = next_pid();
  std::shared_ptr<Object> obj = std::make_shared<Object>();
  if (const Object* const extra = as_object(payload)) {
    for (const std::u16string& key : object_keys(payload)) obj->set(key, field_or(payload, key));
  }
  obj->set(u"is_req", Value(true));
  obj->set(u"pid", Value(to_u16(pid)));
  obj->set(u"type", Value(type));
  const std::optional<std::u16string> text = json_stringify(Value(obj));
  if (!text.has_value() || text->empty()) {
    if (_log) _log(std::string("json empty for ") + to_u8(type));
    return pid;
  }
  if (_transport != nullptr) _transport->send(to_u8(*text));
  return pid;
}

void NetClient::request(const std::u16string& type, const Value& payload, double timeout_ms,
                        lfw::net::NetOk ok, lfw::net::NetFail fail) {
  if (!opened()) {
    if (fail) fail(u"not open");
    return;
  }
  const std::string pid = send_req(type, payload);
  Job job;
  job.ok = std::move(ok);
  job.fail = std::move(fail);
  if (timeout_ms > 0.0) {
    job.has_deadline = true;
    job.deadline = now_ms() + timeout_ms;
  }
  _jobs[pid] = std::move(job);
}

void NetClient::send_nowait(const std::u16string& type, const Value& payload) {
  if (!opened()) return;
  static_cast<void>(send_req(type, payload));
}

void NetClient::notify_error(const std::u16string& text) {
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_error(text);
}

void NetClient::open(const std::u16string& url) {
  _dead = false;
  cancel_reconnect();
  _urls.clear();
  std::string raw = to_u8(url);
  const auto trim = [](std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
  };
  raw = trim(raw);
  const bool has_protocol = raw.rfind("wss://", 0) == 0 || raw.rfind("ws://", 0) == 0 ||
                            raw.rfind("https://", 0) == 0 || raw.rfind("http://", 0) == 0;
  if (has_protocol) {
    _urls.push_back(raw);
  } else {
    for (const char* const protocol : {"wss://", "ws://", "https://", "http://"}) {
      _urls.push_back(std::string(protocol) + raw);
    }
  }
  try_next_url();
}

void NetClient::try_next_url() {
  if (_urls.empty()) {
    if (_transport != nullptr) _transport->connect(std::string());
    return;
  }
  _last_url = _urls.front();
  _urls.erase(_urls.begin());
  _connecting = true;
  _connect_deadline = now_ms() + kConnectTimeoutMs;
  if (_log) _log("connect " + _last_url);
  _transport->connect(_last_url);
}

void NetClient::close() {
  cancel_reconnect();
  _rejoin_enabled = false;
  _dead = true;
  _ever_open = false;
  _connecting = false;
  if (_transport != nullptr) _transport->connect(std::string());
  fail_all_jobs();
  handle_close(true);
}

void NetClient::give_up() {
  if (opened() && _room.has_value() && _client.has_value() && !_secret.empty()) {
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"roomid", _room->id);
    payload->set(u"client_id", Value(_client->id));
    payload->set(u"secret", Value(_secret));
    send_nowait(u"Abandon", Value(payload));
  }
  close();
}

void NetClient::fail_all_jobs() {
  std::map<std::string, Job> jobs;
  jobs.swap(_jobs);
  for (auto& kv : jobs) {
    if (kv.second.fail) kv.second.fail(u"closed");
  }
}

void NetClient::submit_client() {
  std::shared_ptr<Object> payload = std::make_shared<Object>();
  payload->set(u"name", Value(_nickname));
  std::vector<Value> players;
  players.reserve(_players.size());
  for (const std::u16string& p : _players) players.push_back(Value(p));
  payload->set(u"players", Value(std::make_shared<Array>(std::move(players))));
  request(u"ClientInfo", Value(payload), 1000.0,
          [this](const Value& resp) {
            const Value client = field_or(resp, u"client");
            if (as_object(client) != nullptr) _client = parse_client(client);
            const std::u16string secret = text_of(resp, u"secret");
            if (!_reconnecting && !secret.empty()) _secret = secret;
            emit_room();
          },
          [this](const std::u16string&) { close(); });
}

void NetClient::poll() {
  if (_transport == nullptr) return;
  _transport->poll();

  if (_transport->connected()) {
    if (!_ever_open) {
      _ever_open = true;
      _connecting = false;
      handle_open();
    }
  } else if (_ever_open) {
    _ever_open = false;
    handle_close(false);
  } else if (_connecting && now_ms() >= _connect_deadline) {
    _connecting = false;
    if (!_urls.empty()) {
      try_next_url();
    } else {
      handle_close(false);
    }
  }

  std::vector<std::string> inbox;
  if (_transport->drain(inbox)) {
    for (const std::string& text : inbox) handle_text(text);
  }

  const double now = now_ms();
  std::vector<std::string> expired;
  for (const auto& kv : _jobs) {
    if (kv.second.has_deadline && now >= kv.second.deadline) expired.push_back(kv.first);
  }
  for (const std::string& pid : expired) {
    const auto it = _jobs.find(pid);
    if (it == _jobs.end()) continue;
    lfw::net::NetFail fail = std::move(it->second.fail);
    _jobs.erase(it);
    notify_error(u"request timeout");
    if (fail) fail(u"timeout");
  }

  if (opened() && now >= _next_ping) {
    _next_ping = now + kPingIntervalMs;
    send_ping();
  }

  if (_reconnecting && _reconnect_at > 0.0 && now >= _reconnect_at) {
    _reconnect_at = 0.0;
    _connecting = true;
    _connect_deadline = now + kConnectTimeoutMs;
    _transport->connect(_last_url);
  }
}

void NetClient::handle_open() {
  if (_log) _log("open");
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_open();
  std::shared_ptr<Object> payload = std::make_shared<Object>();
  payload->set(u"name", Value(_nickname));
  std::vector<Value> players;
  players.reserve(_players.size());
  for (const std::u16string& p : _players) players.push_back(Value(p));
  payload->set(u"players", Value(std::make_shared<Array>(std::move(players))));
  request(u"ClientInfo", Value(payload), 1000.0,
          [this](const Value& resp) {
            const Value client = field_or(resp, u"client");
            if (as_object(client) != nullptr) _client = parse_client(client);
            const std::u16string secret = text_of(resp, u"secret");
            if (!_reconnecting && !secret.empty()) _secret = secret;
            _next_ping = 0.0;
            emit_room();
            if (_reconnecting) submit_rejoin();
          },
          [this](const std::u16string&) { close(); });
}

void NetClient::handle_close(bool intentional) {
  if (_log) _log(intentional ? "close (intentional)" : "close");
  _connecting = false;
  if (!intentional && !_urls.empty()) {
    try_next_url();
    return;
  }
  if (!intentional && _rejoin_enabled && _room.has_value() && _client.has_value()) {
    schedule_reconnect();
    return;
  }
  if (!_urls.empty()) {
    try_next_url();
    return;
  }
  _dead = true;
  if (_room.has_value()) {
    _room.reset();
    for (lfw::net::INetListener* const listener : _listeners) listener->on_net_room(nullptr);
  }
  if (!_rooms.empty()) {
    _rooms.clear();
    emit_rooms();
  }
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_close(std::u16string());
}

void NetClient::schedule_reconnect() {
  if (_reconnecting && _reconnect_at > 0.0) return;
  _reconnecting = true;
  if (_resume_client_id.empty() && _client.has_value()) {
    _resume_client_id = to_u8(_client->id);
  }
  _reconnect_attempt += 1;
  if (_reconnect_attempt > kMaxReconnectAttempts) {
    _reconnecting = false;
    _rejoin_enabled = false;
    for (lfw::net::INetListener* const listener : _listeners) {
      listener->on_net_rejoin_failed(false);
    }
    return;
  }
  for (lfw::net::INetListener* const listener : _listeners) {
    listener->on_net_reconnecting(_reconnect_attempt);
  }
  const double delay =
      std::min(kReconnectMaxMs, kReconnectBaseMs * static_cast<double>(_reconnect_attempt));
  _reconnect_at = now_ms() + delay;
}

void NetClient::cancel_reconnect() {
  _reconnecting = false;
  _reconnect_attempt = 0;
  _reconnect_at = 0.0;
}

void NetClient::submit_rejoin() {
  const Value room_id = _room.has_value() ? _room->id : Value();
  const std::u16string client_id = !_resume_client_id.empty()
                                       ? to_u16(_resume_client_id)
                                       : (_client.has_value() ? _client->id : std::u16string());
  if (std::holds_alternative<std::monostate>(room_id) ||
      std::holds_alternative<NullTag>(room_id) || client_id.empty() || _secret.empty()) {
    handle_close(true);
    return;
  }
  std::shared_ptr<Object> payload = std::make_shared<Object>();
  payload->set(u"roomid", room_id);
  payload->set(u"client_id", Value(client_id));
  payload->set(u"secret", Value(_secret));
  payload->set(u"from_seq", Value(static_cast<double>(_rejoin_next_seq)));
  request(u"Rejoin", Value(payload), 5000.0,
          [this](const Value& resp) {
            std::vector<Value> frames;
            if (const Array* const arr = as_array(field_or(resp, u"resps"))) {
              for (const Value& item : arr->items()) frames.push_back(item);
            }
            pull_rejoin_frames(resp, frames.size(), std::move(frames));
          },
          [this](const std::u16string&) {
            cancel_reconnect();
            _rejoin_enabled = false;
            for (lfw::net::INetListener* const listener : _listeners) {
              listener->on_net_rejoin_failed(true);
            }
          });
}

void NetClient::pull_rejoin_frames(const Value& first, size_t index, std::vector<Value> acc) {
  const bool done = bool_of(first, u"done", true);
  if (done) {
    cancel_reconnect();
    _resume_client_id.clear();
    const Value client = field_or(first, u"client");
    if (as_object(client) != nullptr) _client = parse_client(client);
    const Value room = field_or(first, u"room");
    if (as_object(room) != nullptr) {
      _room = parse_room(room);
      emit_room();
    }
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"resps", Value(std::make_shared<Array>(std::move(acc))));
    for (lfw::net::INetListener* const listener : _listeners) {
      listener->on_net_rejoin(Value(payload));
    }
    return;
  }
  std::shared_ptr<Object> payload = std::make_shared<Object>();
  payload->set(u"from_seq", Value(static_cast<double>(num_of(first, u"from_seq", 0.0) +
                                                       static_cast<double>(index))));
  request(u"RejoinFrames", Value(payload), 15000.0,
          [this, first, index, acc = std::move(acc)](const Value& chunk) mutable {
            const Array* const arr = as_array(field_or(chunk, u"resps"));
            if (arr == nullptr || arr->empty()) {
              pull_rejoin_frames(first, acc.size(), std::move(acc));
              return;
            }
            size_t added = 0;
            for (const Value& item : arr->items()) {
              acc.push_back(item);
              ++added;
            }
            std::shared_ptr<Object> merged = std::make_shared<Object>(*as_object(first));
            merged->set(u"done", Value(bool_of(chunk, u"done", false)));
            pull_rejoin_frames(Value(merged), index + added, std::move(acc));
          },
          [this](const std::u16string&) {
            _reconnecting = true;
            _reconnect_at = 0.0;
            schedule_reconnect();
          });
}

void NetClient::send_ping() {
  std::shared_ptr<Object> payload = std::make_shared<Object>();
  payload->set(u"time", Value(now_ms()));
  if (_rtt > 0.0) payload->set(u"rtt", Value(_rtt));
  request(u"Ping", Value(payload), 0.0, nullptr, nullptr);
}

void NetClient::handle_text(const std::string& utf8) {
  const std::optional<Value> root = json_parse(to_u16(utf8));
  if (!root.has_value() || as_object(*root) == nullptr) return;
  if (truthy(field_or(*root, u"is_resp"))) {
    handle_resp(*root);
    return;
  }
}

void NetClient::handle_resp(const Value& resp) {
  const std::string pid = to_u8(text_of(resp, u"pid"));
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_message(resp);

  const std::u16string type = text_of(resp, u"type");
  const bool has_code = truthy(field_or(resp, u"code"));
  if (_log && type != u"Tick" && type != u"Ping") {
    _log("resp " + to_u8(type) + (pid.empty() ? " (push)" : " pid=" + pid));
  }
  if (!has_code) update_room_from_resp(resp);

  const auto it = _jobs.find(pid);
  if (it == _jobs.end()) return;
  Job job = std::move(it->second);
  _jobs.erase(it);
  if (has_code) {
    const std::u16string error = text_of(resp, u"error");
    if (_log) _log("error " + to_u8(type) + ": " + to_u8(error));
    if (job.fail) job.fail(error.empty() ? u"error" : error);
    return;
  }
  if (job.ok) job.ok(resp);
}

void NetClient::update_room_from_resp(const Value& resp) {
  const std::u16string type = text_of(resp, u"type");
  const Value room = field_or(resp, u"room");
  const bool has_room = as_object(room) != nullptr;
  if (type == u"JoinRoom" || type == u"CreateRoom" || type == u"RoomSync") {
    if (has_room) {
      _room = parse_room(room);
      emit_room();
    }
    return;
  }
  if (type == u"Rejoin") {
    if (has_room) {
      _room = parse_room(room);
      emit_room();
    }
    return;
  }
  if (type == u"CloseRoom") {
    _room.reset();
    for (lfw::net::INetListener* const listener : _listeners) listener->on_net_room(nullptr);
    return;
  }
  if (type == u"ExitRoom" || type == u"Kick") {
    const Value client = field_or(resp, u"client");
    const std::u16string client_id = text_of(client, u"id");
    const bool me = _client.has_value() && same_text(client_id, _client->id);
    if (me) {
      _room.reset();
      for (lfw::net::INetListener* const listener : _listeners) listener->on_net_room(nullptr);
    } else if (has_room) {
      _room = parse_room(room);
      emit_room();
    }
    return;
  }
  if (type == u"ClientReady" || type == u"ClientInfo") {
    if (!_room.has_value()) return;
    const Value client = field_or(resp, u"client");
    if (as_object(client) == nullptr) return;
    const lfw::net::NetClientInfo info = parse_client(client);
    for (lfw::net::NetRoomClient& member : _room->clients) {
      if (same_text(member.info.id, info.id)) {
        if (type == u"ClientReady") {
          member.ready = bool_of(resp, u"ready");
        } else {
          member.info = info;
        }
      }
    }
    if (same_text(_room->owner.info.id, info.id) && type == u"ClientInfo") {
      _room->owner.info = info;
    }
    emit_room();
    return;
  }
  if (type == u"ListRooms") {
    _rooms.clear();
    if (const Array* const arr = as_array(field_or(resp, u"rooms"))) {
      for (const Value& item : arr->items()) _rooms.push_back(parse_room(item));
    }
    emit_rooms();
    return;
  }
  if (type == u"Ping") {
    const std::u16string client = text_of(resp, u"client");
    if (_client.has_value() && same_text(client, _client->id)) {
      _rtt = std::max(0.0, now_ms() - num_of(resp, u"time", now_ms()));
    }
    for (lfw::net::INetListener* const listener : _listeners) listener->on_net_ping(_rtt);
    return;
  }
  if (type == u"RoomStart" && _room_start_sink) {
    _room_start_sink(resp);
    return;
  }
  if (_frame_sink && (type == u"Tick" || type == u"KeyTick")) {
    _frame_sink(resp);
  }
}

void NetClient::emit_room() {
  const lfw::net::NetRoomInfo* room = _room.has_value() ? &_room.value() : nullptr;
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_room(room);
}

void NetClient::emit_rooms() {
  for (lfw::net::INetListener* const listener : _listeners) listener->on_net_rooms(_rooms);
}

}  // namespace lfw::host::net
