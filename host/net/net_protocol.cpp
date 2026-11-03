#include "host/net/net_protocol.h"

#include <memory>
#include <utility>

#include "lfw/core/json.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"

namespace lfw::host::net {

namespace {

constexpr const char16_t* kKeyIsReq = u"is_req";
constexpr const char16_t* kKeyPid = u"pid";
constexpr const char16_t* kKeyType = u"type";
constexpr const char16_t* kKeySeq = u"seq";
constexpr const char16_t* kKeyCmds = u"cmds";
constexpr const char16_t* kKeyReqs = u"reqs";
constexpr const char16_t* kKeyClientId = u"client_id";
constexpr const char16_t* kKeyChecksum = u"_s";
constexpr const char16_t* kKeySeed = u"seed";
constexpr const char16_t* kKeySyncMode = u"sync_mode";
constexpr const char16_t* kKeyInputDelay = u"input_delay";

std::u16string to_u16(const std::string& utf8) {
  return decode_utf8(std::vector<uint8_t>(utf8.begin(), utf8.end()));
}

std::string to_u8(const std::u16string& text) {
  const std::vector<uint8_t> buf = encode_utf8(text);
  return std::string(buf.begin(), buf.end());
}

const std::u16string* as_text(const Value& v) { return std::get_if<std::u16string>(&v); }

Value text_value(const std::string& text) { return Value(to_u16(text)); }

std::string text_of(const Value& holder, const char16_t* key) {
  const Value v = field_or(holder, key);
  const std::u16string* const s = as_text(v);
  return s != nullptr ? to_u8(*s) : std::string();
}

}  // namespace

const char* msg_type_name(MsgType type) {
  switch (type) {
    case MsgType::kClientInfo: return "ClientInfo";
    case MsgType::kCreateRoom: return "CreateRoom";
    case MsgType::kJoinRoom: return "JoinRoom";
    case MsgType::kExitRoom: return "ExitRoom";
    case MsgType::kClientReady: return "ClientReady";
    case MsgType::kRoomStart: return "RoomStart";
    case MsgType::kRoomSync: return "RoomSync";
    case MsgType::kListRooms: return "ListRooms";
    case MsgType::kTick: return "Tick";
    case MsgType::kPing: return "Ping";
    case MsgType::kError: return "Error";
    case MsgType::kUnknown: break;
  }
  return "Unknown";
}

MsgType parse_msg_type(const std::u16string& name) {
  for (MsgType t = MsgType::kClientInfo; t <= MsgType::kError; t = static_cast<MsgType>(static_cast<int>(t) + 1)) {
    const char* const ascii = msg_type_name(t);
    std::u16string wide;
    for (const char* p = ascii; *p != '\0'; ++p) wide.push_back(static_cast<char16_t>(*p));
    if (wide == name) return t;
  }
  return MsgType::kUnknown;
}

std::string encode_envelope(const Msg& msg) {
  auto obj = std::make_shared<Object>();
  if (const Object* const extra = as_object(msg.payload); extra != nullptr) {
    for (const std::u16string& key : object_keys(msg.payload)) obj->set(key, field_or(msg.payload, key));
  }
  obj->set(kKeyIsReq, Value(msg.is_req));
  obj->set(kKeyPid, text_value(msg.pid));
  std::u16string type = to_u16(msg_type_name(msg.type));
  obj->set(kKeyType, Value(type));
  const std::optional<std::u16string> text = json_stringify(Value(obj));
  return text.has_value() ? to_u8(*text) : std::string();
}

std::optional<Msg> decode_envelope(const std::string& utf8) {
  if (utf8.empty()) return std::nullopt;
  const std::optional<Value> root = json_parse(to_u16(utf8));
  if (!root.has_value() || as_object(*root) == nullptr) return std::nullopt;
  Msg msg;
  msg.is_req = truthy(field_or(*root, kKeyIsReq));
  msg.pid = text_of(*root, kKeyPid);
  const Value type_value = field_or(*root, kKeyType);
  const std::u16string* const type = as_text(type_value);
  msg.type = type != nullptr ? parse_msg_type(*type) : MsgType::kUnknown;
  auto payload = std::make_shared<Object>();
  for (const std::u16string& key : object_keys(*root)) {
    if (key == kKeyIsReq || key == kKeyPid || key == kKeyType) continue;
    payload->set(key, field_or(*root, key));
  }
  msg.payload = Value(payload);
  return msg;
}

Value make_tick_req(int seq, const std::vector<std::string>& cmds, const std::string& checksum) {
  auto obj = std::make_shared<Object>();
  obj->set(kKeySeq, Value(static_cast<double>(seq)));
  std::vector<Value> items;
  items.reserve(cmds.size());
  for (const std::string& cmd : cmds) items.push_back(text_value(cmd));
  obj->set(kKeyCmds, Value(std::make_shared<Array>(std::move(items))));
  if (!checksum.empty()) obj->set(kKeyChecksum, text_value(checksum));
  return Value(obj);
}

std::optional<ClientTick> parse_tick_req(const Msg& msg) {
  ClientTick out;
  out.seq = static_cast<int>(to_number(field_or(msg.payload, kKeySeq)));
  out.client_id = text_of(msg.payload, kKeyClientId);
  out.checksum = text_of(msg.payload, kKeyChecksum);
  const Value cmds_value = field_or(msg.payload, kKeyCmds);
  const Array* const cmds = as_array(cmds_value);
  if (cmds != nullptr) {
    out.cmds.reserve(cmds->size());
    for (const Value& item : cmds->items()) {
      const std::u16string* const s = as_text(item);
      if (s != nullptr) out.cmds.push_back(to_u8(*s));
    }
  }
  return out;
}

Value make_tick_resp(int seq, const std::vector<ClientTick>& reqs) {
  auto obj = std::make_shared<Object>();
  obj->set(kKeySeq, Value(static_cast<double>(seq)));
  std::vector<Value> items;
  items.reserve(reqs.size());
  for (const ClientTick& req : reqs) {
    auto one = std::make_shared<Object>();
    one->set(kKeyClientId, text_value(req.client_id));
    one->set(kKeySeq, Value(static_cast<double>(req.seq)));
    std::vector<Value> cmds;
    cmds.reserve(req.cmds.size());
    for (const std::string& cmd : req.cmds) cmds.push_back(text_value(cmd));
    one->set(kKeyCmds, Value(std::make_shared<Array>(std::move(cmds))));
    if (!req.checksum.empty()) one->set(kKeyChecksum, text_value(req.checksum));
    items.push_back(Value(one));
  }
  obj->set(kKeyReqs, Value(std::make_shared<Array>(std::move(items))));
  return Value(obj);
}

std::optional<FrameInputs> parse_tick_resp(const Msg& msg) {
  FrameInputs out;
  out.seq = static_cast<int>(to_number(field_or(msg.payload, kKeySeq)));
  const Value reqs_value = field_or(msg.payload, kKeyReqs);
  const Array* const reqs = as_array(reqs_value);
  if (reqs == nullptr) return out;
  out.reqs.reserve(reqs->size());
  for (const Value& item : reqs->items()) {
    ClientTick one;
    one.client_id = text_of(item, kKeyClientId);
    one.seq = static_cast<int>(to_number(field_or(item, kKeySeq)));
    one.checksum = text_of(item, kKeyChecksum);
    const Value cmds_value = field_or(item, kKeyCmds);
    const Array* const cmds = as_array(cmds_value);
    if (cmds != nullptr) {
      one.cmds.reserve(cmds->size());
      for (const Value& cmd : cmds->items()) {
        const std::u16string* const s = as_text(cmd);
        if (s != nullptr) one.cmds.push_back(to_u8(*s));
      }
    }
    out.reqs.push_back(std::move(one));
  }
  return out;
}

Value make_room_start_resp(double seed, const SyncPlan& plan) {
  auto obj = std::make_shared<Object>();
  obj->set(kKeySeed, Value(seed));
  std::u16string mode;
  for (const char* p = sync_mode_name(plan.mode); *p != '\0'; ++p) mode.push_back(static_cast<char16_t>(*p));
  obj->set(kKeySyncMode, Value(mode));
  obj->set(kKeyInputDelay, Value(static_cast<double>(plan.input_delay)));
  return Value(obj);
}

std::optional<RoomStartInfo> parse_room_start(const Msg& msg) {
  RoomStartInfo out;
  out.seed = to_number(field_or(msg.payload, kKeySeed));
  const Value mode_value = field_or(msg.payload, kKeySyncMode);
  const std::u16string* const mode = as_text(mode_value);
  if (mode != nullptr) parse_sync_mode(*mode, out.plan.mode);
  const double delay = to_number(field_or(msg.payload, kKeyInputDelay));
  out.plan.input_delay = delay > 0.0 ? static_cast<int>(delay) : 1;
  return out;
}

}
