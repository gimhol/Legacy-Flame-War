#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/keyed_map.h"

namespace lfw {

/**
 * `ISoundPlayInfo`（`src/LFW/defines/IStagePhaseInfo.ts`）：阶段开场音效。
 *
 * `x/y/z` 缺省是 NaN（TS 是 `undefined`），`sounds_play` 那边 `to_number`
 * 之后同样是 NaN，语义一致。
 */
class SoundPlayInfo {
 public:
  SoundPlayInfo() = default;
  explicit SoundPlayInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& path() const { return _path; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  const std::u16string& desc() const { return _desc; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _path;
  double _x = std::numeric_limits<double>::quiet_NaN();
  double _y = std::numeric_limits<double>::quiet_NaN();
  double _z = std::numeric_limits<double>::quiet_NaN();
  std::u16string _desc;
};

/**
 * `IStageObjectInfo`（`src/LFW/defines/IStageObjectInfo.ts`）：阶段里刷的一批物件。
 *
 * 数值缺省全是 NaN —— 调用方一律按「NaN 就是没写」判断（`x` 没写时在
 * `enemy_l`/`enemy_r` 之间随机，`hp` 没写时按 `hp_map`……）。
 * `act` / `join_team` / `outline_color` 用 `optional`，因为它们要区分
 * 「没写」和「写了空串」。
 */
class StageObjectInfo {
 public:
  StageObjectInfo() = default;
  explicit StageObjectInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::vector<std::u16string>& id() const { return _id; }
  const std::u16string& id_method() const { return _id_method; }
  const std::optional<std::u16string>& act() const { return _act; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double range_x() const { return _range_x; }
  double range_y() const { return _range_y; }
  double range_z() const { return _range_z; }
  double facing() const { return _facing; }
  double hp() const { return _hp; }
  double mp() const { return _mp; }
  const KeyedMap& hp_map() const { return _hp_map; }
  const KeyedMap& mp_map() const { return _mp_map; }
  double times() const { return _times; }
  double ratio() const { return _ratio; }
  double reserve() const { return _reserve; }
  bool is_boss() const { return _is_boss.value_or(false); }
  bool is_soldier() const { return _is_soldier.value_or(false); }
  double join() const { return _join; }
  const std::optional<std::u16string>& join_team() const { return _join_team; }
  double join_reserve() const { return _join_reserve; }
  const std::optional<std::u16string>& outline_color() const { return _outline_color; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::vector<std::u16string> _id;
  std::u16string _id_method;
  std::optional<std::u16string> _act;
  double _x = std::numeric_limits<double>::quiet_NaN();
  double _y = std::numeric_limits<double>::quiet_NaN();
  double _z = std::numeric_limits<double>::quiet_NaN();
  double _range_x = std::numeric_limits<double>::quiet_NaN();
  double _range_y = std::numeric_limits<double>::quiet_NaN();
  double _range_z = std::numeric_limits<double>::quiet_NaN();
  double _facing = std::numeric_limits<double>::quiet_NaN();
  double _hp = std::numeric_limits<double>::quiet_NaN();
  double _mp = std::numeric_limits<double>::quiet_NaN();
  KeyedMap _hp_map;
  KeyedMap _mp_map;
  double _times = std::numeric_limits<double>::quiet_NaN();
  double _ratio = std::numeric_limits<double>::quiet_NaN();
  double _reserve = std::numeric_limits<double>::quiet_NaN();
  std::optional<bool> _is_boss;
  std::optional<bool> _is_soldier;
  double _join = std::numeric_limits<double>::quiet_NaN();
  std::optional<std::u16string> _join_team;
  double _join_reserve = std::numeric_limits<double>::quiet_NaN();
  std::optional<std::u16string> _outline_color;
};

/**
 * `IStagePhaseInfo`（`src/LFW/defines/IStagePhaseInfo.ts`）：一个关卡阶段。
 *
 * `title` / `music` 用 `optional`：前者要区分「没写」与「写了空串」，
 * 后者空串表示停止当前 BGM。`respawn*` / `health_up` / `mp_up` 是按难度
 * 取值的字典，交给 `KeyedMap`。`dialogs` 还没具体化（`IDialogInfo` 下一批），
 * `__end_testers` 是 cook 产物，都按原样给出。
 */
class StagePhaseInfo {
 public:
  StagePhaseInfo() = default;
  explicit StagePhaseInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::optional<std::u16string>& title() const { return _title; }
  const std::u16string& desc() const { return _desc; }
  const std::optional<std::u16string>& music() const { return _music; }
  double bound() const { return _bound; }
  double player_l() const { return _player_l; }
  double player_r() const { return _player_r; }
  double camera_l() const { return _camera_l; }
  double camera_r() const { return _camera_r; }
  double enemy_l() const { return _enemy_l; }
  double enemy_r() const { return _enemy_r; }
  double drink_l() const { return _drink_l; }
  double drink_r() const { return _drink_r; }
  double cam_jump_to_x() const { return _cam_jump_to_x; }
  double player_jump_to_x() const { return _player_jump_to_x; }
  double player_jump_to_z() const { return _player_jump_to_z; }
  double player_facing() const { return _player_facing; }
  double hide_stats() const { return _hide_stats; }
  double world_pause() const { return _world_pause; }
  double control_disabled() const { return _control_disabled; }
  double weapon_rain_disabled() const { return _weapon_rain_disabled; }
  const std::vector<std::u16string>& end_test() const { return _end_test; }
  const std::vector<std::u16string>& on_start() const { return _on_start; }
  const std::vector<std::u16string>& on_end() const { return _on_end; }
  const KeyedMap& respawn() const { return _respawn; }
  const KeyedMap& respawn_r() const { return _respawn_r; }
  const KeyedMap& respawn_x() const { return _respawn_x; }
  const KeyedMap& health_up() const { return _health_up; }
  const KeyedMap& mp_up() const { return _mp_up; }
  const std::vector<SoundPlayInfo>& sounds() const { return _sounds; }
  const std::vector<StageObjectInfo>& objects() const { return _objects; }
  const Value& dialogs() const { return _dialogs; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::optional<std::u16string> _title;
  std::u16string _desc;
  std::optional<std::u16string> _music;
  double _bound = std::numeric_limits<double>::quiet_NaN();
  double _player_l = std::numeric_limits<double>::quiet_NaN();
  double _player_r = std::numeric_limits<double>::quiet_NaN();
  double _camera_l = std::numeric_limits<double>::quiet_NaN();
  double _camera_r = std::numeric_limits<double>::quiet_NaN();
  double _enemy_l = std::numeric_limits<double>::quiet_NaN();
  double _enemy_r = std::numeric_limits<double>::quiet_NaN();
  double _drink_l = std::numeric_limits<double>::quiet_NaN();
  double _drink_r = std::numeric_limits<double>::quiet_NaN();
  double _cam_jump_to_x = std::numeric_limits<double>::quiet_NaN();
  double _player_jump_to_x = std::numeric_limits<double>::quiet_NaN();
  double _player_jump_to_z = std::numeric_limits<double>::quiet_NaN();
  double _player_facing = std::numeric_limits<double>::quiet_NaN();
  double _hide_stats = std::numeric_limits<double>::quiet_NaN();
  double _world_pause = std::numeric_limits<double>::quiet_NaN();
  double _control_disabled = std::numeric_limits<double>::quiet_NaN();
  double _weapon_rain_disabled = std::numeric_limits<double>::quiet_NaN();
  std::vector<std::u16string> _end_test;
  std::vector<std::u16string> _on_start;
  std::vector<std::u16string> _on_end;
  KeyedMap _respawn;
  KeyedMap _respawn_r;
  KeyedMap _respawn_x;
  KeyedMap _health_up;
  KeyedMap _mp_up;
  std::vector<SoundPlayInfo> _sounds;
  std::vector<StageObjectInfo> _objects;
  Value _dialogs;
};

}
