#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/entity/summary.h"

namespace lfw {

class Entity;

class SummaryMgr {
 public:
  std::shared_ptr<Summary> get(const std::u16string& id);
  void clear();
  void release(const std::u16string& id);
  void add_damage_sum(const Entity& a, const Value& value);
  void add_kill_sum(const Entity& a, const Value& value = Value());
  void apply_damage(const Entity& a, const Value& injury, const Entity& v, const Value& prev_hp);

  const std::vector<std::pair<std::u16string, std::shared_ptr<Summary>>>& items() const {
    return _items;
  }
  std::size_t grave_count() const { return _graves.size(); }

 protected:
  std::shared_ptr<Summary> acquire(const std::u16string& id);

 private:
  // `_items` 保持插入顺序（UI 要按顺序读），另存一张 id → 下标的表做 O(1) 查找。
  std::shared_ptr<Summary>* find_item(std::u16string_view id);
  void reindex();

  struct KeyHash {
    using is_transparent = void;
    size_t operator()(std::u16string_view s) const {
      return std::hash<std::u16string_view>{}(s);
    }
  };
  struct KeyEq {
    using is_transparent = void;
    bool operator()(std::u16string_view a, std::u16string_view b) const { return a == b; }
  };

  std::vector<std::shared_ptr<Summary>> _graves;
  std::vector<std::pair<std::u16string, std::shared_ptr<Summary>>> _items;
  std::unordered_map<std::u16string, std::size_t, KeyHash, KeyEq> _index;
};

SummaryMgr& summary_mgr();

}
