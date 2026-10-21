#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IBdyInfo`（`src/LFW/defines/IBdyInfo.ts`）：受击区。
 *
 * 形状（`IQube`: x/y/w/h/z/l）与 kind/hit_flag 是具体值；`actions`、`__tester`、
 * `__indicator_info` 这类由动作系统 / 表达式引擎消费的仍是 `Value`。
 */
class BdyInfo {
 public:
  BdyInfo() = default;
  explicit BdyInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  double x() const { return _x; }
  double y() const { return _y; }
  double w() const { return _w; }
  double h() const { return _h; }
  double z() const { return _z; }
  double l() const { return _l; }
  double kind() const { return _kind; }
  double hit_flag() const { return _hit_flag; }
  const std::u16string& kind_name() const { return _kind_name; }
  const std::u16string& hit_flag_name() const { return _hit_flag_name; }
  const std::u16string& prefab_id() const { return _prefab_id; }
  const std::u16string& test() const { return _test; }
  const Value& actions() const { return _actions; }
  const Value& on_hit_ground() const { return _on_hit_ground; }
  const Value& tester() const { return _tester; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  double _x = 0;
  double _y = 0;
  double _w = 0;
  double _h = 0;
  double _z = 0;
  double _l = 0;
  double _kind = 0;
  double _hit_flag = 0;
  std::u16string _kind_name;
  std::u16string _hit_flag_name;
  std::u16string _prefab_id;
  std::u16string _test;
  Value _actions;
  Value _on_hit_ground;
  Value _tester;
  Value _indicator_info;
};

/**
 * `IItrInfo`（`src/LFW/defines/IItrInfo.ts`）：攻击判定区。
 *
 * 与 `BdyInfo` 同理：形状与伤害/击退参数是具体值，`actions` / `catchingact` /
 * `caughtact` / `__tester` 仍是 `Value`（动作与表达式引擎消费）。
 * `interval*` 是 cook 阶段加的重复触发参数。
 */
class ItrInfo {
 public:
  ItrInfo() = default;
  explicit ItrInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  double x() const { return _x; }
  double y() const { return _y; }
  double w() const { return _w; }
  double h() const { return _h; }
  double z() const { return _z; }
  double l() const { return _l; }
  double kind() const { return _kind; }
  double hit_flag() const { return _hit_flag; }
  const std::u16string& kind_name() const { return _kind_name; }
  const std::u16string& hit_flag_name() const { return _hit_flag_name; }
  double motionless() const { return _motionless; }
  double shaking() const { return _shaking; }
  double dvx() const { return _dvx; }
  double dvy() const { return _dvy; }
  double dvz() const { return _dvz; }
  double fall() const { return _fall; }
  double vrest() const { return _vrest; }
  double arest() const { return _arest; }
  double bdefend() const { return _bdefend; }
  double injury() const { return _injury; }
  double effect() const { return _effect; }
  const std::u16string& effect_name() const { return _effect_name; }
  const std::u16string& prefab_id() const { return _prefab_id; }
  const std::u16string& test() const { return _test; }
  double interval() const { return _interval; }
  double interval_mode() const { return _interval_mode; }
  const Value& interval_id() const { return _interval_id; }
  const Value& actions() const { return _actions; }
  const Value& catchingact() const { return _catchingact; }
  const Value& caughtact() const { return _caughtact; }
  const Value& on_hit_ground() const { return _on_hit_ground; }
  const Value& tester() const { return _tester; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  double _x = 0;
  double _y = 0;
  double _w = 0;
  double _h = 0;
  double _z = 0;
  double _l = 0;
  double _kind = 0;
  double _hit_flag = 0;
  std::u16string _kind_name;
  std::u16string _hit_flag_name;
  double _motionless = 0;
  double _shaking = 0;
  double _dvx = 0;
  double _dvy = 0;
  double _dvz = 0;
  double _fall = 0;
  double _vrest = 0;
  double _arest = 0;
  double _bdefend = 0;
  double _injury = 0;
  double _effect = 0;
  std::u16string _effect_name;
  std::u16string _prefab_id;
  std::u16string _test;
  double _interval = 0;
  double _interval_mode = 0;
  Value _interval_id;
  Value _actions;
  Value _catchingact;
  Value _caughtact;
  Value _on_hit_ground;
  Value _tester;
  Value _indicator_info;
};

/** 把 `[Value]`（或单个 `Value`）解析成 `std::vector<T>`。 */
template <typename T>
std::vector<T> read_list(const Value& list) {
  std::vector<T> out;
  if (const Array* const arr = as_array(list)) {
    out.reserve(arr->size());
    for (const Value& v : arr->items()) out.push_back(T(v));
  }
  return out;
}

}