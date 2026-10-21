#pragma once

#include <string>

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
  explicit NextFrame(const Value& raw);

  bool valid() const;
  bool is_str() const;
  bool is_list() const;
  /** 整个值就是帧 id（`string` 形态）。 */
  const std::u16string& str() const;

  /** `id?: string | string[]`。 */
  const Value& id() const;
  double hp() const { return _hp; }
  double mp() const { return _mp; }
  double mp_mode() const { return _mp_mode; }

  const Value& to_value() const;

 private:
  const Value* _raw = nullptr;
  double _hp = 0;
  double _mp = 0;
  double _mp_mode = 0;
};

}