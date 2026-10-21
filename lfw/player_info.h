#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/core/value.h"
#include "lfw/data/keyed_map.h"

namespace lfw {

class Entity;

struct PlayerInfoCacheEntry {
  bool missing = false;
  bool get_threw = false;

  enum class DataKind { kFalsy, kBytes, kOther };
  DataKind data_kind = DataKind::kFalsy;
  std::vector<uint8_t> data;
  Value data_other;

  enum class BlobKind { kFalsy, kBytes, kThrows };
  BlobKind blob_kind = BlobKind::kFalsy;
  std::vector<uint8_t> blob;
};

struct PlayerInfoCachePut {
  std::u16string name;
  std::u16string type;
  double version = 0;
  std::vector<uint8_t> data;
};

class IPlayerInfoHost {
 public:
  virtual ~IPlayerInfoHost() = default;
  virtual void cache_get(const std::u16string& name, PlayerInfoCacheEntry& out) = 0;
  virtual bool cache_del(const std::u16string& name, std::u16string& error) = 0;
  virtual void cache_put(const PlayerInfoCachePut& data) = 0;
  virtual void warn(const std::u16string& text) = 0;
};

class PlayerInfo {
 public:
  static constexpr const char16_t* kTag = u"PlayerInfo";
  static constexpr const char16_t* kDataType = u"PlayerInfo";
  static constexpr double kDataVersion = 1;

  PlayerInfo(IPlayerInfoHost* host, const std::u16string& id, const Value& name, const Value& local,
             const Value& mine);

  Callbacks callbacks;

  const Value& local() const { return _local; }
  const Value& mine() const { return _mine; }
  bool loaded();

  const Value& info() const { return _info; }
  std::u16string id() const;
  std::u16string storage_key() const;
  Value name() const;
  /** `IPurePlayerInfo.keys`：`Record<GK, string>`。 */
  KeyedMap keys() const;
  bool is_com() const { return _is_com; }
  Value ctrl() const;
  Entity* fighter() const { return _fighter; }

  void set_name(const Value& name, bool emit);
  void set_ctrl(const Value& ctrl, bool emit);
  void set_is_com(bool is_com, bool emit);
  bool set_key(const Value& name, const Value& key, bool emit, std::u16string& error);
  bool get_key(const Value& name, Value& out, std::u16string& error) const;
  void set_fighter(Entity* v) { _fighter = v; }

  bool load();
  void save();

 private:
  Value field(const char16_t* key) const;
  void set_field(const char16_t* key, const Value& v);
  void warn(const std::u16string& text);
  bool load_impl();
  void flush_pending_load();

  IPlayerInfoHost* _host = nullptr;
  Value _info;
  Value _local;
  Value _mine;
  bool _loaded = false;
  bool _load_pending = false;
  bool _is_com = false;
  Entity* _fighter = nullptr;
};

Value player_view_of(const PlayerInfo* p);

}
