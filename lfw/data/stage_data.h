#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/stage_phase_info.h"

namespace lfw {

/**
 * `IStageInfo`（`src/LFW/defines/IStageInfo.ts`）：`data/*.stage.json5` 里的一项。
 *
 * `title` / `chapter` / `next` / `bg` 用 `optional`：调用方要区分
 * 「没写」和「写了空串」（比如 `next` 没写就没有下一关）。
 */
class StageData {
 public:
  StageData() = default;
  explicit StageData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  const std::optional<std::u16string>& title() const { return _title; }
  const std::optional<std::u16string>& chapter() const { return _chapter; }
  const std::optional<std::u16string>& next() const { return _next; }
  const std::optional<std::u16string>& bg() const { return _bg; }
  const std::optional<std::u16string>& cond_end() const { return _cond_end; }
  const std::optional<std::u16string>& act_of_goto_next() const { return _act_of_goto_next; }
  bool is_starting() const { return _is_starting.value_or(false); }
  const std::u16string& starting_name() const { return _starting_name; }
  const std::vector<std::u16string>& group() const { return _group; }
  const std::vector<StagePhaseInfo>& phases() const { return _phases; }

  /** 表达式引擎 / JS 桥专用。业务代码别用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const StageData& other) const;

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  std::optional<std::u16string> _title;
  std::optional<std::u16string> _chapter;
  std::optional<std::u16string> _next;
  std::optional<std::u16string> _bg;
  std::optional<std::u16string> _cond_end;
  std::optional<std::u16string> _act_of_goto_next;
  std::optional<bool> _is_starting;
  std::u16string _starting_name;
  std::vector<std::u16string> _group;
  std::vector<StagePhaseInfo> _phases;
};

}
