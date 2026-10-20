#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

class WorldDataset {
 public:
  using FieldHook = std::function<void(const Value&, const Value&)>;
  using ChangeCallback =
      std::function<void(const std::u16string&, const Value&, const Value&)>;

  static WorldDataset& default_instance();

  explicit WorldDataset(bool pure = false);

  bool has(const std::u16string& key) const;
  bool tracked(const std::u16string& key) const;
  Value get(const std::u16string& key) const;
  void set(const std::u16string& key, Value v);
  const std::vector<std::u16string>& keys() const { return _keys; }
  std::vector<std::u16string> enumerable_keys() const;

  void set_field_hook(const std::u16string& key, FieldHook hook);
  ChangeCallback on_dataset_change;

  Value dump_dataset() const;

 private:
  void add(const char16_t* key, double value);
  void install_defaults();

  bool _pure = false;
  std::vector<std::u16string> _keys;
  std::map<std::u16string, Value> _values;
  std::set<std::u16string> _tracked;
  std::map<std::u16string, FieldHook> _hooks;
};

}
