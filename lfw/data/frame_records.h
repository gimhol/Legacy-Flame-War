#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/vec3_scale.h"

namespace lfw {

/**
 * 帧上的几个定长小记录：`ICpoint` / `IWpointInfo` / `IBpointInfo` / `IChaseInfo`
 * （`src/LFW/defines/*.ts`）。数值按 `to_number` 语义（缺失是 NaN），
 * 只有指针类的 `__indicator_info` 保持 `Value`。
 */
class Cpoint {
 public:
  Cpoint() = default;
  explicit Cpoint(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  double kind() const { return _kind; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double injury() const { return _injury; }
  double hurtable() const { return _hurtable; }
  double decrease() const { return _decrease; }
  double throwvx() const { return _throwvx; }
  double throwvy() const { return _throwvy; }
  double throwvz() const { return _throwvz; }
  double throwinjury() const { return _throwinjury; }
  double shaking() const { return _shaking; }
  double motionless() const { return _motionless; }
  const std::u16string& fronthurtact() const { return _fronthurtact; }
  const std::u16string& backhurtact() const { return _backhurtact; }
  const Value& vaction() const { return _vaction; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  double _kind = 0;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  double _injury = 0;
  double _hurtable = 0;
  double _decrease = 0;
  double _throwvx = 0;
  double _throwvy = 0;
  double _throwvz = 0;
  double _throwinjury = 0;
  double _shaking = 0;
  double _motionless = 0;
  std::u16string _fronthurtact;
  std::u16string _backhurtact;
  Value _vaction;
  Value _indicator_info;
};

class Wpoint {
 public:
  Wpoint() = default;
  explicit Wpoint(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  double kind() const { return _kind; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double dvx() const { return _dvx; }
  double dvy() const { return _dvy; }
  double dvz() const { return _dvz; }
  const std::u16string& weaponact() const { return _weaponact; }
  const std::u16string& attacking() const { return _attacking; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  double _kind = 0;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  // 没写 wpoint 的帧拿到的是默认构造的 Wpoint：`dvx/dvy/dvz` 必须保持 NaN，
  // 否则 `follow_bearer` 会把「没有 wpoint」当成「wpoint 写了 dvx」而立刻把武器扔出去。
  double _dvx = std::numeric_limits<double>::quiet_NaN();
  double _dvy = std::numeric_limits<double>::quiet_NaN();
  double _dvz = std::numeric_limits<double>::quiet_NaN();
  std::u16string _weaponact;
  std::u16string _attacking;
  Value _indicator_info;
};

class Bpoint {
 public:
  Bpoint() = default;
  explicit Bpoint(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double r() const { return _r; }
  const Value& indicator_info() const { return _indicator_info; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  double _r = 0;
  Value _indicator_info;
};

class ChaseInfo {
 public:
  ChaseInfo() = default;
  explicit ChaseInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double stratedy() const { return _stratedy; }
  double flag() const { return _flag; }
  double lost() const { return _lost; }
  double oy() const { return _oy; }
  const Vec3Scale& overshoot() const { return _overshoot; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  double _stratedy = 0;
  double _flag = 0;
  double _lost = 0;
  double _oy = 0;
  Vec3Scale _overshoot;
};


/** `string | string[]`（例如帧的 `sound`）。 */
class StringOrList {
 public:
  StringOrList() = default;
  explicit StringOrList(Value raw);

  bool valid() const { return !std::holds_alternative<std::monostate>(_raw); }
  bool is_list() const { return as_array(_raw) != nullptr; }
  const std::u16string& str() const { return _str; }
  const std::vector<std::u16string>& list() const { return _list; }
  /** 单值也归一成 1 项的列表。 */
  const std::vector<std::u16string>& items() const;

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _str;
  std::vector<std::u16string> _list;
};

/** `IFrameModel`（`src/LFW/defines/IFrameModel.ts`）：帧的 3D 模型视觉。 */
class FrameModel {
 public:
  FrameModel() = default;
  explicit FrameModel(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& anim() const { return _anim; }
  const std::vector<std::u16string>& bones() const { return _bones; }
  const std::vector<double>& pos() const { return _pos; }
  const std::vector<double>& rot() const { return _rot; }
  const std::vector<double>& scl() const { return _scl; }
  double seek() const { return _seek; }
  double seek_end() const { return _seek_end; }
  double time_scale() const { return _time_scale; }
  double rad() const { return _rad; }
  std::optional<bool> hold_last() const { return _hold_last; }
  std::optional<bool> loop() const { return _loop; }
  std::optional<bool> reverse() const { return _reverse; }
  const Value& pose() const { return _pose; }
  const Value& rotation() const { return _rotation; }
  const Value& scale() const { return _scale; }
  const Value& offset() const { return _offset; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _anim;
  std::vector<std::u16string> _bones;
  std::vector<double> _pos;
  std::vector<double> _rot;
  std::vector<double> _scl;
  double _seek = 0;
  double _seek_end = 0;
  double _time_scale = 0;
  double _rad = 0;
  std::optional<bool> _hold_last;
  std::optional<bool> _loop;
  std::optional<bool> _reverse;
  Value _pose;
  Value _rotation;
  Value _scale;
  Value _offset;
};

}
