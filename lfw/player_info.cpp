#include "lfw/player_info.h"

#include "lfw/utils/container_help/field_or.h"

#include <cmath>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/defines/ctrl_device.h"
#include "lfw/defines/defines_data.h"
#include "lfw/utils/type_check.h"
#include "lfw/utils/utf8.h"

namespace lfw {

namespace {

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

bool is_nullish(const Value& v) {
  return is_undefined(v) || std::holds_alternative<NullTag>(v);
}

bool prop_get(const Value& o, const Value& name, Value& out, std::u16string& error) {
  if (is_nullish(o)) {
    error = u"cannot read property of nullish";
    return false;
  }
  const std::u16string key = to_string(name);
  if (const std::u16string* s = std::get_if<std::u16string>(&o)) {
    if (key == u"length") {
      out = Value(static_cast<double>(s->size()));
      return true;
    }
    uint32_t idx = 0;
    if (is_array_index(key, idx)) {
      out = idx < s->size() ? Value(std::u16string(1, (*s)[idx])) : Value();
      return true;
    }
    out = Value();
    return true;
  }
  if (const Array* arr = as_array(o)) {
    if (key == u"length") {
      out = Value(static_cast<double>(arr->size()));
      return true;
    }
    uint32_t idx = 0;
    if (is_array_index(key, idx)) {
      out = idx < arr->size() ? arr->at(idx) : Value();
      return true;
    }
    out = Value();
    return true;
  }
  if (const Object* obj = as_object(o)) {
    const Value* f = obj->get(key);
    out = f != nullptr ? *f : Value();
    return true;
  }
  out = Value();
  return true;
}

bool prop_set(Value& o, const Value& name, const Value& v, std::u16string& error) {
  const std::u16string key = to_string(name);
  if (Object* obj = as_object(o)) {
    obj->set(key, v);
    return true;
  }
  if (Array* arr = as_array(o)) {
    if (key == u"length") {
      const double n = to_number(v);
      if (!(n >= 0) || n > 1000000.0 || n != std::floor(n)) {
        error = u"unsupported array length";
        return false;
      }
      while (arr->size() > static_cast<size_t>(n)) arr->remove_at(arr->size() - 1);
      while (arr->size() < static_cast<size_t>(n)) arr->push_back(Value());
      return true;
    }
    uint32_t idx = 0;
    if (is_array_index(key, idx)) {
      if (idx > 1000000) {
        error = u"unsupported array index";
        return false;
      }
      while (arr->size() <= idx) arr->push_back(Value());
      arr->at(idx) = v;
      return true;
    }
    error = u"unsupported named property on array";
    return false;
  }
  error = u"cannot create property on non-object";
  return false;
}

std::vector<std::u16string> for_in_keys(const Value& v) {
  std::vector<std::u16string> out;
  if (const Object* obj = as_object(v)) return obj->keys();
  if (const Array* arr = as_array(v)) {
    for (size_t i = 0; i < arr->size(); ++i) out.push_back(number_to_string(static_cast<double>(i)));
    return out;
  }
  if (const std::u16string* s = std::get_if<std::u16string>(&v)) {
    for (size_t i = 0; i < s->size(); ++i) out.push_back(number_to_string(static_cast<double>(i)));
  }
  return out;
}

bool destructure(const Value& o, const Value& key, const Value* fallback, Value& out,
                 std::u16string& error) {
  if (is_nullish(o)) {
    error = u"cannot destructure nullish";
    return false;
  }
  if (!prop_get(o, key, out, error)) return false;
  if (is_undefined(out) && fallback != nullptr) out = *fallback;
  return true;
}

}

Value PlayerInfo::field(const char16_t* key) const {
  const Object* info = as_object(_info);
  if (info == nullptr) return Value();
  const Value* f = info->get(key);
  return f != nullptr ? *f : Value();
}

void PlayerInfo::set_field(const char16_t* key, const Value& v) {
  Object* info = as_object(_info);
  if (info != nullptr) info->set(key, v);
}

void PlayerInfo::warn(const std::u16string& text) { _host->warn(text); }

Value player_view_of(const PlayerInfo* const p) {
  if (p == nullptr) return Value();
  auto o = std::make_shared<Object>();
  const Value& info = p->info();
  o->set(u"id", field_or(info, u"id"));
  o->set(u"name", field_or(info, u"name"));
  o->set(u"keys", field_or(info, u"keys"));
  o->set(u"version", field_or(info, u"version"));
  o->set(u"ctrl", field_or(info, u"ctrl"));
  o->set(u"local", p->local());
  o->set(u"mine", p->mine());
  o->set(u"is_com", Value(p->is_com()));
  return Value(o);
}

PlayerInfo::PlayerInfo(IPlayerInfoHost* host, const std::u16string& id, const Value& name,
                       const Value& local, const Value& mine)
    : _host(host), _local(local), _mine(mine) {
  if (is_undefined(_local)) _local = Value(true);
  if (is_undefined(_mine)) _mine = Value(true);
  Value name_v = name;
  if (is_undefined(name_v)) name_v = Value(id);

  _info = Value(std::make_shared<Object>());
  Object* const info = as_object(_info);
  info->set(u"id", Value(id));
  info->set(u"name", name_v);
  info->set(u"keys", defines::get_default_keys_value(id));
  info->set(u"version", Value(0.0));
  info->set(u"ctrl", Value(static_cast<double>(CtrlDevice::Keyboard)));
  _load_pending = true;
}

bool PlayerInfo::loaded() {
  flush_pending_load();
  return _loaded;
}

void PlayerInfo::flush_pending_load() {
  if (!_load_pending) return;
  _load_pending = false;
  _loaded = load_impl();
}

std::u16string PlayerInfo::id() const {
  const Value v = field(u"id");
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : std::u16string();
}

std::u16string PlayerInfo::storage_key() const { return u"player_info_" + id(); }

Value PlayerInfo::name() const { return field(u"name"); }
KeyedMap PlayerInfo::keys() const { return KeyedMap(field(u"keys")); }
Value PlayerInfo::ctrl() const { return field(u"ctrl"); }

void PlayerInfo::set_name(const Value& name, bool emit) {
  const Value prev = field(u"name");
  if (strict_equals(prev, name)) return;
  set_field(u"name", name);
  if (emit) callbacks.call(u"on_name_changed", {name, prev});
}

void PlayerInfo::set_ctrl(const Value& ctrl, bool emit) {
  const Value prev = field(u"ctrl");
  if (strict_equals(prev, ctrl)) return;
  set_field(u"ctrl", ctrl);
  if (emit) callbacks.call(u"on_ctrl_changed", {ctrl, prev});
}

void PlayerInfo::set_is_com(bool is_com, bool emit) {
  if (_is_com == is_com) return;
  _is_com = is_com;
  if (emit) callbacks.call(u"on_is_com_changed", {Value(is_com)});
}

bool PlayerInfo::set_key(const Value& name, const Value& key, bool emit, std::u16string& error) {
  Value keys_v = keys().to_value();
  Value prev;
  if (!prop_get(keys_v, name, prev, error)) return false;
  if (strict_equals(prev, key)) return true;

  const std::u16string* const k = std::get_if<std::u16string>(&key);
  if (k == nullptr) {
    error = u"key.toLowerCase is not a function";
    return false;
  }
  const Value lowered(std::u16string(to_lower_case(*k)));
  if (!prop_set(keys_v, name, lowered, error)) return false;
  if (emit) callbacks.call(u"on_key_changed", {name, lowered, prev});
  return true;
}

bool PlayerInfo::get_key(const Value& name, Value& out, std::u16string& error) const {
  return prop_get(keys().to_value(), name, out, error);
}

bool PlayerInfo::load() {
  flush_pending_load();
  return load_impl();
}

bool PlayerInfo::load_impl() {
  if (!truthy(_local)) return false;

  PlayerInfoCacheEntry cache;
  _host->cache_get(storage_key(), cache);
  if (cache.get_threw) {
    warn(u"[PlayerInfo::load] failed to load, reason");
    return false;
  }
  if (cache.missing) return false;

  std::vector<uint8_t> data;
  if (cache.data_kind == PlayerInfoCacheEntry::DataKind::kBytes) {
    data = cache.data;
  } else if (cache.data_kind == PlayerInfoCacheEntry::DataKind::kOther) {
    warn(u"[PlayerInfo::load] load failed, reason");
    return false;
  } else {
    if (cache.blob_kind == PlayerInfoCacheEntry::BlobKind::kFalsy) {
      warn(u"[PlayerInfo::load] no data");
      return false;
    }
    if (cache.blob_kind == PlayerInfoCacheEntry::BlobKind::kThrows) {
      warn(u"[PlayerInfo::load] read blob failed, reason: ");
      return false;
    }
    data = cache.blob;
  }

  std::u16string error;
  const std::u16string raw_text = decode_utf8(data);
  const Json5Result parsed = json5_parse(raw_text);
  if (!parsed.ok) {
    warn(u"[PlayerInfo::load] load failed, reason");
    return false;
  }
  const Value raw_info = parsed.value;

  Value name_v;
  Value keys_v;
  Value ctrl_v;
  Value version_v;
  const Value ctrl_default = ctrl();
  if (!destructure(raw_info, Value(u"name"), nullptr, name_v, error) ||
      !destructure(raw_info, Value(u"keys"), nullptr, keys_v, error) ||
      !destructure(raw_info, Value(u"ctrl"), &ctrl_default, ctrl_v, error) ||
      !destructure(raw_info, Value(u"version"), nullptr, version_v, error)) {
    warn(u"[PlayerInfo::load] load failed, reason");
    return false;
  }

  if (!strict_equals(version_v, field(u"version"))) {
    warn(u"[PlayerInfo::load] version changed");
    return false;
  }
  if (is_str(name_v)) set_name(name_v, true);
  if (truthy(keys_v)) {
    for (const std::u16string& k : for_in_keys(keys_v)) {
      Value kv;
      if (!prop_get(keys_v, Value(k), kv, error)) {
        warn(u"[PlayerInfo::load] load failed, reason");
        return false;
      }
      if (!set_key(Value(k), kv, true, error)) {
        warn(u"[PlayerInfo::load] load failed, reason");
        return false;
      }
    }
  }
  if (!strict_equals(ctrl_v, ctrl())) set_ctrl(ctrl_v, true);
  return true;
}

void PlayerInfo::save() {
  if (!truthy(_local)) return;

  std::u16string error;
  if (!_host->cache_del(storage_key(), error)) {
    warn(u"[PlayerInfo::load]");
    return;
  }
  const std::optional<std::u16string> text = json_stringify(_info);
  if (!text.has_value()) {
    warn(u"[PlayerInfo::load]");
    return;
  }
  PlayerInfoCachePut put;
  put.name = storage_key();
  put.type = kDataType;
  put.version = kDataVersion;
  put.data = encode_utf8(*text);
  _host->cache_put(put);
}

}
