#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IDialogInfo`（`src/LFW/defines/IDialogInfo.ts`）：一句台词。
 *
 * `pause` / `close_by` / `hide_stats` / `end_test` 的数据包里目前还没人读，
 * 但按 TS 的形状保留下来；`__end_testers` 是 cook 产物，走 `to_value()`。
 */
class DialogInfo {
 public:
  DialogInfo() = default;
  explicit DialogInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& type() const { return _type; }
  const std::optional<std::u16string>& fighter() const { return _fighter; }
  const std::optional<bool>& pause() const { return _pause; }
  const std::u16string& i18n() const { return _i18n; }
  const std::optional<std::u16string>& close_by() const { return _close_by; }
  double hide_stats() const { return _hide_stats; }
  const std::vector<std::u16string>& end_test() const { return _end_test; }

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const DialogInfo& other) const;

 private:
  Value _raw;
  std::u16string _type;
  std::optional<std::u16string> _fighter;
  std::optional<bool> _pause;
  std::u16string _i18n;
  std::optional<std::u16string> _close_by;
  double _hide_stats = std::numeric_limits<double>::quiet_NaN();
  std::vector<std::u16string> _end_test;
};

}
