#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

enum HitFieldBit : std::uint32_t {
  kHasX = 1u << 0,
  kHasY = 1u << 1,
  kHasW = 1u << 2,
  kHasH = 1u << 3,
  kHasZ = 1u << 4,
  kHasL = 1u << 5,
  kHasKind = 1u << 6,
  kHasHitFlag = 1u << 7,
  kHasMotionless = 1u << 8,
  kHasShaking = 1u << 9,
  kHasDvx = 1u << 10,
  kHasDvy = 1u << 11,
  kHasDvz = 1u << 12,
  kHasFall = 1u << 13,
  kHasVrest = 1u << 14,
  kHasArest = 1u << 15,
  kHasBdefend = 1u << 16,
  kHasInjury = 1u << 17,
  kHasEffect = 1u << 18,
  kHasInterval = 1u << 19,
  kHasIntervalMode = 1u << 20,
  kHasCode = 1u << 21,
};
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
  bool has_kind() const { return (_has & kHasKind) != 0; }
  bool has_hit_flag() const { return (_has & kHasHitFlag) != 0; }
  bool has_x() const { return (_has & kHasX) != 0; }
  bool has_y() const { return (_has & kHasY) != 0; }
  bool has_w() const { return (_has & kHasW) != 0; }
  bool has_h() const { return (_has & kHasH) != 0; }
  bool has_z() const { return (_has & kHasZ) != 0; }
  bool has_l() const { return (_has & kHasL) != 0; }
  std::uint32_t presence() const { return _has; }
  double code() const { return _code; }
  bool has_code() const { return (_has & kHasCode) != 0; }
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
  std::uint32_t _has = 0;
  double _code = 0;
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
  bool has_kind() const { return (_has & kHasKind) != 0; }
  bool has_hit_flag() const { return (_has & kHasHitFlag) != 0; }
  bool has_x() const { return (_has & kHasX) != 0; }
  bool has_y() const { return (_has & kHasY) != 0; }
  bool has_w() const { return (_has & kHasW) != 0; }
  bool has_h() const { return (_has & kHasH) != 0; }
  bool has_z() const { return (_has & kHasZ) != 0; }
  bool has_l() const { return (_has & kHasL) != 0; }
  std::uint32_t presence() const { return _has; }
  double code() const { return _code; }
  bool has_code() const { return (_has & kHasCode) != 0; }
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
  bool has_motionless() const { return (_has & kHasMotionless) != 0; }
  bool has_shaking() const { return (_has & kHasShaking) != 0; }
  bool has_dvx() const { return (_has & kHasDvx) != 0; }
  bool has_dvy() const { return (_has & kHasDvy) != 0; }
  bool has_dvz() const { return (_has & kHasDvz) != 0; }
  bool has_fall() const { return (_has & kHasFall) != 0; }
  bool has_vrest() const { return (_has & kHasVrest) != 0; }
  bool has_arest() const { return (_has & kHasArest) != 0; }
  bool has_bdefend() const { return (_has & kHasBdefend) != 0; }
  bool has_injury() const { return (_has & kHasInjury) != 0; }
  bool has_effect() const { return (_has & kHasEffect) != 0; }
  bool has_interval() const { return (_has & kHasInterval) != 0; }
  bool has_interval_mode() const { return (_has & kHasIntervalMode) != 0; }  const Value& interval_id() const { return _interval_id; }
  const Value& actions() const { return _actions; }
  const Value& catchingact() const { return _catchingact; }
  const Value& caughtact() const { return _caughtact; }
  const Value& on_hit_ground() const { return _on_hit_ground; }
  const Value& tester() const { return _tester; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:

  Value _raw;
  std::uint32_t _has = 0;
  double _code = 0;
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

struct HitBoxShape {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
  double z = 0;
  double l = 0;
  std::uint32_t has = 0;
};

template <typename T>
HitBoxShape box_shape_of(const T& t) {
  HitBoxShape s;
  s.x = t.x();
  s.y = t.y();
  s.w = t.w();
  s.h = t.h();
  s.z = t.z();
  s.l = t.l();
  s.has = t.presence();
  return s;
}

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