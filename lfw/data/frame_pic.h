#pragma once

#include <optional>
#include <string>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IFramePic`（`src/LFW/defines/IFramePic.ts`）：帧切图。
 *
 * `tex + (x,y,w,h)` 是必填，旋转/变换中心是可选项；`__cos_r` / `__sin_r` 是
 * cook 阶段算出来的内部量，仍是 `Value`（`todo`）。
 */
class FramePic {
 public:
  FramePic() = default;
  explicit FramePic(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& tex() const { return _tex; }
  double x() const { return _x; }
  double y() const { return _y; }
  double w() const { return _w; }
  double h() const { return _h; }
  std::optional<double> rad() const { return _rad; }
  std::optional<double> deg() const { return _deg; }
  std::optional<double> ox() const { return _ox; }
  std::optional<double> oy() const { return _oy; }
  std::optional<double> cx() const { return _cx; }
  std::optional<double> cy() const { return _cy; }
  const Value& cos_r() const { return _cos_r; }
  const Value& sin_r() const { return _sin_r; }

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _tex;
  double _x = 0;
  double _y = 0;
  double _w = 0;
  double _h = 0;
  std::optional<double> _rad;
  std::optional<double> _deg;
  std::optional<double> _ox;
  std::optional<double> _oy;
  std::optional<double> _cx;
  std::optional<double> _cy;
  Value _cos_r;
  Value _sin_r;
};

}