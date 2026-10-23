#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `{[key: string]: any}` 形态的字典（帧上的 `dataset` 覆盖表、`__seq_map`
 * 按键序列表都是这个形状）。
 *
 * 键是运行期算出来的，所以不可能是定长 struct；这里把"按键取值"的四种写法
 * 收敛成一份 API（`find` / `get` / `num` / `has` / `keys`），调用方不再散落
 * `field_or(map, key.c_str())`。
 */
class KeyedMap {
 public:
  KeyedMap() = default;
  explicit KeyedMap(Value raw) : _raw(std::move(raw)) {}

  bool valid() const { return as_object(_raw) != nullptr; }

  const Value* find(std::u16string_view key) const;
  const Value* find(const char16_t* key) const { return find(std::u16string_view(key)); }
  /** 缺失时返回 `undefined`。 */
  const Value& get(std::u16string_view key) const;
  /** 缺失或非数 → NaN（与 `to_number(field_or(...))` 一致）。 */
  double num(std::u16string_view key) const;
  bool has(std::u16string_view key) const { return find(key) != nullptr; }
  std::vector<std::u16string> keys() const;

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
};

}