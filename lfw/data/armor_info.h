#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IArmorInfo`（`src/LFW/defines/IArmorInfo.ts`）：护甲/抗性数据。
 *
 * 数值一律按 `to_number` 语义（字段缺失 → NaN），`fulltime` 是 `optional<bool>`
 * （缺失与 `false` 在判定里等价，但保留原来的三态）。`armor` 本身可以不存在，
 * 用 `valid()` 判断（对应原来的 `truthy(armor)`）。
 */
class ArmorInfo {
 public:
  ArmorInfo() = default;
  explicit ArmorInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  double type() const { return _type; }
  double toughness() const { return _toughness; }
  double toughness_r_value() const { return _toughness_r_value; }
  double toughness_r_tick() const { return _toughness_r_tick; }
  double toughness_resting() const { return _toughness_resting; }
  double fireproof() const { return _fireproof; }
  double antifreeze() const { return _antifreeze; }
  std::optional<bool> fulltime() const { return _fulltime; }
  double injury_ratio() const { return _injury_ratio; }
  double shaking_ratio() const { return _shaking_ratio; }
  double motionless_ratio() const { return _motionless_ratio; }
  const std::vector<std::u16string>& hit_sounds() const { return _hit_sounds; }
  const std::vector<std::u16string>& dead_sounds() const { return _dead_sounds; }
  /** 字段是否真的存在（`[]` 与“没写”在旧代码里行为不同：空数组是 truthy）。 */
  bool has_hit_sounds() const { return _has_hit_sounds; }
  bool has_dead_sounds() const { return _has_dead_sounds; }

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  double _type = 0;
  double _toughness = 0;
  double _toughness_r_value = 0;
  double _toughness_r_tick = 0;
  double _toughness_resting = 0;
  double _fireproof = 0;
  double _antifreeze = 0;
  std::optional<bool> _fulltime;
  double _injury_ratio = 0;
  double _shaking_ratio = 0;
  double _motionless_ratio = 0;
  std::vector<std::u16string> _hit_sounds;
  std::vector<std::u16string> _dead_sounds;
  bool _has_hit_sounds = false;
  bool _has_dead_sounds = false;
};

}