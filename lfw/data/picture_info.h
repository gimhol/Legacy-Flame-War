#pragma once

#include <limits>
#include <map>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IVec3Scale`（`src/LFW/defines/IModelInfo.ts`）：三轴数值。
 *
 * `rotation` / `scale` / `offset` 共用这个形状；缺轴是 NaN，整个字段没写时
 * `valid()` 为 false —— 两者的区别照 TS 的 `undefined` vs `{ x: undefined }`。
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

/**
 * `IPictureInfo`（`src/LFW/defines/IPictureInfo.ts`）：`base.files` 里的一张图。
 *
 * `variants` 指向别的图片 id（不同队伍换色）；`row/col/cell_w/cell_h` 是 LF2
 * 遗留字段，只做保留。
 */
class PictureInfo {
 public:
  PictureInfo() = default;
  explicit PictureInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& path() const { return _path; }
  const std::vector<std::u16string>& variants() const { return _variants; }
  double row() const { return _row; }
  double col() const { return _col; }
  double cell_w() const { return _cell_w; }
  double cell_h() const { return _cell_h; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _path;
  std::vector<std::u16string> _variants;
  double _row = std::numeric_limits<double>::quiet_NaN();
  double _col = std::numeric_limits<double>::quiet_NaN();
  double _cell_w = std::numeric_limits<double>::quiet_NaN();
  double _cell_h = std::numeric_limits<double>::quiet_NaN();
};

/**
 * `IModelInfo`（`src/LFW/defines/IModelInfo.ts`）：`base.models` 里的一个 3D 模型。
 *
 * `rotation`（欧拉角，弧度）先于帧上的 `model.rotation` 应用；`scale` 与帧上的
 * 逐轴相乘、`offset` 与帧上的逐轴相加。
 */
class ModelInfo {
 public:
  ModelInfo() = default;
  explicit ModelInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& path() const { return _path; }
  const std::vector<std::u16string>& variants() const { return _variants; }
  const Vec3Scale& rotation() const { return _rotation; }
  const Vec3Scale& scale() const { return _scale; }
  const Vec3Scale& offset() const { return _offset; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _path;
  std::vector<std::u16string> _variants;
  Vec3Scale _rotation;
  Vec3Scale _scale;
  Vec3Scale _offset;
};

using PictureInfoMap = std::map<std::u16string, PictureInfo>;
using ModelInfoMap = std::map<std::u16string, ModelInfo>;

/** 把 `{[id]: IPictureInfo}` / `{[id]: IModelInfo}` 解析成各自的 map。 */
PictureInfoMap read_picture_info_map(const Value& map);
ModelInfoMap read_model_info_map(const Value& map);

}
