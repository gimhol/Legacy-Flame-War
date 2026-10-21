#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IBgInfo`（`src/LFW/defines/IBgInfo.ts`）：背景的整体范围/缩放/影子。
 * 数值缺失是 NaN（`to_number` 语义）。
 */
class BgInfo {
 public:
  BgInfo() = default;
  explicit BgInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& name() const { return _name; }
  const std::u16string& shadow() const { return _shadow; }
  double shadow_w() const { return _shadow_w; }
  double shadow_h() const { return _shadow_h; }
  double left() const { return _left; }
  double right() const { return _right; }
  double far() const { return _far; }
  double near() const { return _near; }
  double height() const { return _height; }
  double zoom_x() const { return _zoom_x; }
  double zoom_y() const { return _zoom_y; }
  double zoom_z() const { return _zoom_z; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _name;
  std::u16string _shadow;
  double _shadow_w = 0;
  double _shadow_h = 0;
  double _left = 0;
  double _right = 0;
  double _far = 0;
  double _near = 0;
  double _height = 0;
  double _zoom_x = 0;
  double _zoom_y = 0;
  double _zoom_z = 0;
};

/**
 * `IBgLayerInfo`（`src/LFW/defines/IBgLayerInfo.ts`）：背景的一层。
 *
 * 数值缺失是 NaN；`has(key)` 用来区分"没写"与"写了 0"
 * （`cc`/`c1`/`c2` 与 `offsetAnimX/Y` 的原逻辑就是按字段存在与否分支的）。
 * `color` 允许 `number | string`，交给颜色解析，保持 `Value`。
 */
class BgLayerInfo {
 public:
  BgLayerInfo() = default;
  explicit BgLayerInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }
  bool has(const char16_t* key) const;
  /** 复制一份并覆盖 x（背景循环铺开时用）。 */
  BgLayerInfo with_x(double x) const;

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  const std::u16string& file() const { return _file; }
  double absolute() const { return _absolute; }
  double opacity() const { return _opacity; }
  double width() const { return _width; }
  double height() const { return _height; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double w() const { return _w; }
  double h() const { return _h; }
  double dw() const { return _dw; }
  double dh() const { return _dh; }
  double uv_loop() const { return _uv_loop; }
  double loop() const { return _loop; }
  double cc() const { return _cc; }
  double c1() const { return _c1; }
  double c2() const { return _c2; }
  double offsetAnimX() const { return _offsetAnimX; }
  double offsetAnimY() const { return _offsetAnimY; }
  const Value& color() const { return _color; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  Value _color;
  std::u16string _id;
  std::u16string _name;
  std::u16string _file;
  double _absolute = 0;
  double _opacity = 0;
  double _width = 0;
  double _height = 0;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  double _w = 0;
  double _h = 0;
  double _dw = 0;
  double _dh = 0;
  double _uv_loop = 0;
  double _loop = 0;
  double _cc = 0;
  double _c1 = 0;
  double _c2 = 0;
  double _offsetAnimX = 0;
  double _offsetAnimY = 0;
};

}
