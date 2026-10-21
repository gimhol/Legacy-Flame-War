#pragma once

#include <optional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/data/hit_key_map.h"

namespace lfw {

/**
 * `IFrameInfo`（`src/LFW/defines/IFrameInfo.ts`）。
 *
 * 帧对象在 cook 之后就不再变化，所以构造时把标量字段全部取成具体类型：
 * 数值一律按 `to_number` 语义（字段缺失 → NaN、`null` → 0），字符串取成 `u16string`。
 * 数组/对象/跳转表达式（`next` / `itr` / `bdy` / `cpoint` …）仍是 `Value`，
 * 它们由碰撞、表达式引擎直接消费。
 *
 * 表达式引擎 / JS 桥要拿整帧时用 `to_value()`。
 */
class FrameInfo {
 public:
  FrameInfo() = default;
  explicit FrameInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }
  /** 字段是否真的存在（`undefined` 与 `null` 的区别要紧的地方用）。 */
  bool has(const char16_t* key) const;

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  const std::u16string& state_name() const { return _state_name; }

  double state() const { return _state; }
  double wait() const { return _wait; }
  double centerx() const { return _centerx; }
  double centery() const { return _centery; }
  double width() const { return _width; }
  double height() const { return _height; }
  double hp() const { return _hp; }
  double mp() const { return _mp; }
  double invisible() const { return _invisible; }
  double invulnerable() const { return _invulnerable; }
  double blinking() const { return _blinking; }
  double bot_ignore() const { return _bot_ignore; }
  double no_shadow() const { return _no_shadow; }
  double jump_flag() const { return _jump_flag; }
  double behavior() const { return _behavior; }
  double facing() const { return _facing; }
  double landable() const { return _landable; }
  double stat_recover() const { return _stat_recover; }
  double toughness_recover() const { return _toughness_recover; }

  double dvx() const { return _dvx; }
  double dvy() const { return _dvy; }
  double dvz() const { return _dvz; }
  double vxm() const { return _vxm; }
  double vym() const { return _vym; }
  double vzm() const { return _vzm; }
  double acc_x() const { return _acc_x; }
  double acc_y() const { return _acc_y; }
  double acc_z() const { return _acc_z; }
  double ctrl_x() const { return _ctrl_x; }
  double ctrl_y() const { return _ctrl_y; }
  double ctrl_z() const { return _ctrl_z; }

  double aabb_x1() const { return _aabb_x1; }
  double aabb_x2() const { return _aabb_x2; }
  double aabb_z1() const { return _aabb_z1; }
  double aabb_z2() const { return _aabb_z2; }

  /** `gravity_enabled?: boolean`（缺失是 `nullopt`）。 */
  std::optional<bool> gravity_enabled() const { return _gravity_enabled; }

  const Value& ref() const { return _ref; }
  const Value& pic() const { return _pic; }
  const Value& pics() const { return _pics; }
  const Value& model() const { return _model; }
  const Value& next() const { return _next; }
  const Value& sound() const { return _sound; }
  const HitKeyMap& hold() const { return _hold; }
  const HitKeyMap& hit() const { return _hit; }
  const HitKeyMap& key_down() const { return _key_down; }
  const HitKeyMap& key_up() const { return _key_up; }
  const Value& seqs() const { return _seqs; }
  const Value& bdy() const { return _bdy; }
  const Value& itr() const { return _itr; }
  const Value& wpoint() const { return _wpoint; }
  const Value& bpoint() const { return _bpoint; }
  const Value& opoint() const { return _opoint; }
  const Value& cpoint() const { return _cpoint; }
  const Value& chase() const { return _chase; }
  const Value& broadcasts() const { return _broadcasts; }
  const Value& dataset() const { return _dataset; }
  const Value& on_dead() const { return _on_dead; }
  const Value& on_exhaustion() const { return _on_exhaustion; }
  const Value& on_landing() const { return _on_landing; }
  const Value& on_restrict() const { return _on_restrict; }
  const Value& on_x_restrict() const { return _on_x_restrict; }
  const Value& on_y_restrict() const { return _on_y_restrict; }
  const Value& on_z_restrict() const { return _on_z_restrict; }
  const Value& indicator_info() const { return _indicator_info; }
  const Value& seq_map() const { return _seq_map; }
  const Value& hit_ground_itrs() const { return _hit_ground_itrs; }
  const Value& hit_ground_bdys() const { return _hit_ground_bdys; }
  const Value& tex() const { return _tex; }

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }
  /** 是不是同一帧对象（改帧时用来判断“没换帧”）。 */
  bool same_as(const FrameInfo& other) const;

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  std::u16string _state_name;
  double _state = 0;
  double _wait = 0;
  double _centerx = 0;
  double _centery = 0;
  double _width = 0;
  double _height = 0;
  double _hp = 0;
  double _mp = 0;
  double _invisible = 0;
  double _invulnerable = 0;
  double _blinking = 0;
  double _bot_ignore = 0;
  double _no_shadow = 0;
  double _jump_flag = 0;
  double _behavior = 0;
  double _facing = 0;
  double _landable = 0;
  double _stat_recover = 0;
  double _toughness_recover = 0;
  double _dvx = 0;
  double _dvy = 0;
  double _dvz = 0;
  double _vxm = 0;
  double _vym = 0;
  double _vzm = 0;
  double _acc_x = 0;
  double _acc_y = 0;
  double _acc_z = 0;
  double _ctrl_x = 0;
  double _ctrl_y = 0;
  double _ctrl_z = 0;
  double _aabb_x1 = 0;
  double _aabb_x2 = 0;
  double _aabb_z1 = 0;
  double _aabb_z2 = 0;
  std::optional<bool> _gravity_enabled;
  Value _ref;
  Value _pic;
  Value _pics;
  Value _model;
  Value _next;
  Value _sound;
  HitKeyMap _hold;
  HitKeyMap _hit;
  HitKeyMap _key_down;
  HitKeyMap _key_up;
  Value _seqs;
  Value _bdy;
  Value _itr;
  Value _wpoint;
  Value _bpoint;
  Value _opoint;
  Value _cpoint;
  Value _chase;
  Value _broadcasts;
  Value _dataset;
  Value _on_dead;
  Value _on_exhaustion;
  Value _on_landing;
  Value _on_restrict;
  Value _on_x_restrict;
  Value _on_y_restrict;
  Value _on_z_restrict;
  Value _indicator_info;
  Value _seq_map;
  Value _hit_ground_itrs;
  Value _hit_ground_bdys;
  Value _tex;
};

/** `defines/EMPTY_FRAME_INFO`：`BallController.frame` 的初值（TS 里是个冻结字面量）。 */
const FrameInfo& empty_frame_info();
/** `defines/GONE_FRAME_INFO`：实体离场时的帧。 */
const FrameInfo& gone_frame_info();

}