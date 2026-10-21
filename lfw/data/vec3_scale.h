#pragma once

#include <limits>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IVec3Scale`（`src/LFW/defines/IModelInfo.ts`）/ `IChaseOvershoot` 这类三轴小对象。
 *
 * 缺轴是 NaN；整个字段没写时 `valid()` 为 false —— 区分 TS 的
 * `undefined` 与 `{ x: undefined }`。
 */
class Vec3Scale {
 public:
  Vec3Scale() = default;
  explicit Vec3Scale(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  double _x = std::numeric_limits<double>::quiet_NaN();
  double _y = std::numeric_limits<double>::quiet_NaN();
  double _z = std::numeric_limits<double>::quiet_NaN();
};

}
