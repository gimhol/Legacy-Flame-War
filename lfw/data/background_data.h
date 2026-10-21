#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

/** 背景数据（cook 后的 `.bg.xml` → `bg_data`）。 */
class BackgroundData {
 public:
  BackgroundData() = default;
  explicit BackgroundData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const Object& base() const;
  const Object& dataset() const;
  const Value& layers() const { return _layers; }

  /** 表达式引擎 / JS 桥专用。业务代码别用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const BackgroundData& other) const;

 private:
  Value _raw;
  std::u16string _id;
  Value _base;
  Value _dataset;
  Value _layers;
};

}
