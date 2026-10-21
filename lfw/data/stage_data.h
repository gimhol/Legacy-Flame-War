#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

/** 关卡数据（cook 后的 `data/*.stage.json5` 里的一项）。 */
class StageData {
 public:
  StageData() = default;
  explicit StageData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  const Value& title() const { return _title; }
  const Value& chapter() const { return _chapter; }
  const Value& next() const { return _next; }
  const Value& phases() const { return _phases; }
  const Value& bg() const { return _bg; }

  /** 表达式引擎 / JS 桥专用。业务代码别用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const StageData& other) const;

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  Value _title;
  Value _chapter;
  Value _next;
  Value _phases;
  Value _bg;
};

}
