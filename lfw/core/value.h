#pragma once

#include <cmath>
#include <cstdint>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace lfw {

class Array;
class Object;

struct NullTag {};

using Value = std::variant<std::monostate, NullTag, bool, double, std::u16string,
                           std::shared_ptr<Array>, std::shared_ptr<Object>>;

class Array {
 public:
  Array() = default;
  explicit Array(std::vector<Value> items) : _items(std::move(items)) {}

  size_t size() const { return _items.size(); }
  bool empty() const { return _items.empty(); }
  const Value& at(size_t i) const { return _items[i]; }
  Value& at(size_t i) { return _items[i]; }
  void push_back(Value v) { _items.push_back(std::move(v)); }
  void remove_at(size_t i) { _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(i)); }
  const std::vector<Value>& items() const { return _items; }

 private:
  std::vector<Value> _items;
};

class Object {
 public:
  Object() = default;

  size_t size() const { return _ints.size() + _strs.size(); }
  bool empty() const { return _ints.empty() && _strs.empty(); }

  bool has(std::u16string_view key) const;
  const Value* get(std::u16string_view key) const;
  void set(const std::u16string& key, Value v);
  bool remove(const std::u16string& key);
  std::vector<std::u16string> keys() const;

 private:
  // 小对象直接线性扫更快（也省掉一张哈希表的内存）；键一多就建索引，`get` 退化成 O(1)。
  static constexpr size_t kIndexMin = 12;
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
  using Index = std::unordered_map<std::u16string, size_t, KeyHash, KeyEq>;
  void rebuild_index();

  std::map<uint32_t, Value> _ints;
  std::vector<std::pair<std::u16string, Value>> _strs;
  Index _index;
};

bool truthy(const Value& v);
inline bool truthy_number(double v) { return v != 0.0 && !std::isnan(v); }
bool is_array_index(std::u16string_view k, uint32_t& out);
const char* type_of(const Value& v);
bool is_array(const Value& v);
const Array* as_array(const Value& v);
Array* as_array(Value& v);
const Object* as_object(const Value& v);
Object* as_object(Value& v);
std::vector<std::u16string> object_keys(const Value& v);

std::u16string array_join(const Array& a);
Value to_primitive(const Value& v);
double to_number(const Value& v);
std::u16string to_string(const Value& v);
bool strict_equals(const Value& a, const Value& b);
bool equals(const Value& a, const Value& b);

std::optional<bool> less_than(const Value& a, const Value& b);
bool lt(const Value& a, const Value& b);
bool gt(const Value& a, const Value& b);
bool le(const Value& a, const Value& b);
bool ge(const Value& a, const Value& b);

}
