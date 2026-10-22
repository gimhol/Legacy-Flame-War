#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `INextFrame`（`src/LFW/defines/INextFrame.ts`）：一次"跳到哪一帧"的描述。
 *
 * `TNextFrame` 是 `INextFrame | string | string[]`，所以这只是一份**视图**：
 * 字符串/数组形态时 `valid()` 为 false，调用方仍走原来的分支。
 * 判定表达式由 `loader::judger_of` 按引用查表，不属于对象字段。
 */
class NextFrame {
 public:
  NextFrame() = default;
  explicit NextFrame(Value raw);

  bool valid() const;
  bool is_str() const;
  bool is_list() const;
  /** JS 真值语义：`{}` / `"61"` / `["1","2"]` 为 true，`undefined` / `null` / `""` 为 false。 */
  explicit operator bool() const;
  /** 整个值就是帧 id（`string` 形态）。 */
  const std::u16string& str() const;

  /** `id?: string | string[]`，单值归一成 1 项。 */
  const std::vector<std::u16string>& id() const { return _ids; }
  bool has_id() const;
  /** `pick_value` 这类要按原形状做随机选择的边界用。 */
  const Value& id_value() const;
  double hp() const { return _hp; }
  double mp() const { return _mp; }
  double mp_mode() const { return _mp_mode; }

  const Value& to_value() const;

 private:
  Value _raw;
  std::vector<std::u16string> _ids;
  double _hp = 0;
  double _mp = 0;
  double _mp_mode = 0;
};

}