#pragma once

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace helper {

struct Entrant {
  std::u16string uid;
  std::u16string name;
  Value oid;
};

class JoinQueue {
 public:
  static constexpr const char* TAG = "JoinQueue";

  explicit JoinQueue(double cap = 50) : _cap(cap) {}

  double cap() const { return _cap; }
  double size() const { return static_cast<double>(_items.size()); }
  const std::vector<Entrant>& all() const { return _items; }
  bool has(const std::u16string& uid) const { return _uids.count(uid) != 0; }

  bool enqueue(const Entrant& entrant);
  std::optional<Entrant> dequeue();
  bool remove(const std::u16string& uid);
  void clear();

 private:
  std::vector<Entrant> _items;
  std::set<std::u16string> _uids;
  double _cap = 50;
};

Value pick_join_team(const std::vector<std::pair<std::u16string, double>>& counts,
                     const std::vector<std::pair<std::u16string, double>>& caps,
                     const std::u16string* fallen, const std::vector<std::u16string>& order);

}
}
