#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IWorldDataset`（`src/LFW/defines/IWorldDataset.ts`）的数值字段表：键名 == 字段名。
 *
 * `WorldDataset` 本身必须是一张动态表（`dump_dataset` / 字段钩子 / 任意用户键），
 * 所以接口的「具体类型」落在这层访问器上：每个字段一个 `double`，缺失是 NaN
 * （与 TS 的 `to_number(undefined)` 一致）。
 */
#define LFW_WORLD_DATASET_NUMERIC_FIELDS(X) \
  X(itr_fall, u"itr_fall") \
  X(itr_shaking, u"itr_shaking") \
  X(itr_motionless, u"itr_motionless") \
  X(ball_itr_motionless, u"ball_itr_motionless") \
  X(itr_arest, u"itr_arest") \
  X(fvx_f, u"fvx_f") \
  X(fvy_f, u"fvy_f") \
  X(fvz_f, u"fvz_f") \
  X(ivy_f, u"ivy_f") \
  X(ivz_f, u"ivz_f") \
  X(ivx_f, u"ivx_f") \
  X(ivy_d, u"ivy_d") \
  X(ivx_d, u"ivx_d") \
  X(cvy_d, u"cvy_d") \
  X(cvx_d, u"cvx_d") \
  X(tvx_f, u"tvx_f") \
  X(tvy_f, u"tvy_f") \
  X(tvz_f, u"tvz_f") \
  X(begin_blink_time, u"begin_blink_time") \
  X(lying_blink_time, u"lying_blink_time") \
  X(gone_blink_time, u"gone_blink_time") \
  X(vrest_offset, u"vrest_offset") \
  X(min_vrest, u"min_vrest") \
  X(arest_offset, u"arest_offset") \
  X(min_arest, u"min_arest") \
  X(wait_offset, u"wait_offset") \
  X(cha_bc_spd, u"cha_bc_spd") \
  X(cha_bc_tst_spd_x, u"cha_bc_tst_spd_x") \
  X(cha_bc_tst_spd_y, u"cha_bc_tst_spd_y") \
  X(hp_recoverability, u"hp_recoverability") \
  X(hp_r_ticks, u"hp_r_ticks") \
  X(hp_r_value, u"hp_r_value") \
  X(hp_healing_ticks, u"hp_healing_ticks") \
  X(hp_healing_value, u"hp_healing_value") \
  X(mp_healing_ticks, u"mp_healing_ticks") \
  X(mp_healing_value, u"mp_healing_value") \
  X(electrify_duration, u"electrify_duration") \
  X(mp_r_ticks, u"mp_r_ticks") \
  X(mp_r_ratio, u"mp_r_ratio") \
  X(double_click_interval, u"double_click_interval") \
  X(key_hit_duration, u"key_hit_duration") \
  X(friction_factor, u"friction_factor") \
  X(friction_x, u"friction_x") \
  X(friction_z, u"friction_z") \
  X(land_friction_x, u"land_friction_x") \
  X(land_friction_z, u"land_friction_z") \
  X(screen_w, u"screen_w") \
  X(screen_h, u"screen_h") \
  X(gravity, u"gravity") \
  X(gravity_d, u"gravity_d") \
  X(weapon_throwing_gravity, u"weapon_throwing_gravity") \
  X(sync_render, u"sync_render") \
  X(difficulty, u"difficulty") \
  X(infinity_mp, u"infinity_mp") \
  X(fall_r_ticks, u"fall_r_ticks") \
  X(fall_r_value, u"fall_r_value") \
  X(toughness_r_value, u"toughness_r_value") \
  X(defend_r_ticks, u"defend_r_ticks") \
  X(defend_r_value, u"defend_r_value") \
  X(fall_value_max, u"fall_value_max") \
  X(catch_time_max, u"catch_time_max") \
  X(defend_value_max, u"defend_value_max") \
  X(defend_ratio, u"defend_ratio") \
  X(mp_max, u"mp_max") \
  X(hp_max, u"hp_max") \
  X(resting_max, u"resting_max") \
  X(vrest_after_shaking, u"vrest_after_shaking") \
  X(arest_after_motionless, u"arest_after_motionless") \
  X(invisible_blinking, u"invisible_blinking") \
  X(jump_x_f, u"jump_x_f") \
  X(jump_z_f, u"jump_z_f") \
  X(jump_h_f, u"jump_h_f") \
  X(dash_x_f, u"dash_x_f") \
  X(dash_z_f, u"dash_z_f") \
  X(dash_h_f, u"dash_h_f") \
  X(bfall_x_f, u"bfall_x_f") \
  X(bfall_h_f, u"bfall_h_f") \
  X(jump_height, u"jump_height") \
  X(jump_distance, u"jump_distance") \
  X(jump_distancez, u"jump_distancez") \
  X(dash_height, u"dash_height") \
  X(dash_distance, u"dash_distance") \
  X(dash_distancez, u"dash_distancez") \
  X(rowing_height, u"rowing_height") \
  X(rowing_distance, u"rowing_distance") \
  X(wvx_f, u"wvx_f") \
  X(wvy_f, u"wvy_f") \
  X(wvz_f, u"wvz_f") \
  X(whirlwind_vy_max, u"whirlwind_vy_max") \
  X(whirlwind_acc_y, u"whirlwind_acc_y") \
  X(whirlwind_acc_x, u"whirlwind_acc_x") \
  X(whirlwind_acc_z, u"whirlwind_acc_z") \
  X(outline_enabled, u"outline_enabled") \
  X(entity_flags, u"entity_flags") \
  X(bg_flags, u"bg_flags") \
  X(UPS, u"UPS") \
  X(playrate, u"playrate") \
  X(atom_time, u"atom_time")

class WorldDataset {
 public:
  using FieldHook = std::function<void(const Value&, const Value&)>;
  using ChangeCallback =
      std::function<void(const std::u16string&, const Value&, const Value&)>;

  static WorldDataset& default_instance();

  explicit WorldDataset(bool pure = false);

  /** 数值字段：缺失或非数 → NaN（`to_number(undefined)` 同义）。 */
  double num(const std::u16string& key) const;

  bool has(const std::u16string& key) const;
  bool tracked(const std::u16string& key) const;
  Value get(const std::u16string& key) const;

/** 每个 `IWorldDataset` 数值字段一个具名访问器（缺失是 NaN）。 */
#define LFW_WORLD_DATASET_DECL(name, key) double name() const { return num(key); }
  LFW_WORLD_DATASET_NUMERIC_FIELDS(LFW_WORLD_DATASET_DECL)
#undef LFW_WORLD_DATASET_DECL

  void set(const std::u16string& key, Value v);
  const std::vector<std::u16string>& keys() const { return _keys; }
  std::vector<std::u16string> enumerable_keys() const;

  void set_field_hook(const std::u16string& key, FieldHook hook);
  ChangeCallback on_dataset_change;

  Value dump_dataset() const;

 private:
  void add(const char16_t* key, double value);
  void install_defaults();

  bool _pure = false;
  std::vector<std::u16string> _keys;
  std::map<std::u16string, Value> _values;
  std::set<std::u16string> _tracked;
  std::map<std::u16string, FieldHook> _hooks;
};

}
