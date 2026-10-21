#include "lfw/data/stage_phase_info.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {

namespace {

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::numeric_limits<double>::quiet_NaN();
}

std::u16string text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

/** 保留「没写」与「写了 null」的区别：两者在 TS 里都是 nullish，这里都归为 nullopt。 */
std::optional<std::u16string> optional_text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  const std::u16string* const s = std::get_if<std::u16string>(v);
  if (s != nullptr) return *s;
  return std::nullopt;
}

std::optional<bool> optional_bool(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  if (const bool* const b = std::get_if<bool>(v)) return *b;
  if (const double* const n = std::get_if<double>(v)) {
    return std::isfinite(*n) && *n != 0.0;
  }
  return std::nullopt;
}

/**
 * `music` 的三态：没写（nullopt，保持当前 BGM）、`null`/空串（`""`，停止当前 BGM）、
 * 其它（播放）。TS 那里用的是 `!is_undefined(music)` + `truthy(music)`，正好是这个形状。
 */
std::optional<std::u16string> music_text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  if (const std::u16string* const s = std::get_if<std::u16string>(v)) return *s;
  return std::u16string();
}

std::vector<std::u16string> texts(const Object& o, const char16_t* key) {
  std::vector<std::u16string> out;
  const Value* const v = o.get(key);
  if (v == nullptr) return out;
  if (const std::u16string* const one = std::get_if<std::u16string>(v)) {
    out.push_back(*one);
    return out;
  }
  const Array* const arr = as_array(*v);
  if (arr == nullptr) return out;
  out.reserve(arr->size());
  for (const Value& item : arr->items()) {
    const std::u16string* const s = std::get_if<std::u16string>(&item);
    if (s != nullptr) out.push_back(*s);
  }
  return out;
}

}

SoundPlayInfo::SoundPlayInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _path = text(*o, u"path");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _desc = text(*o, u"desc");
}

StageObjectInfo::StageObjectInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = texts(*o, u"id");
  _id_method = text(*o, u"id_method");
  _act = optional_text(*o, u"act");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _range_x = num(*o, u"range_x");
  _range_y = num(*o, u"range_y");
  _range_z = num(*o, u"range_z");
  _facing = num(*o, u"facing");
  _hp = num(*o, u"hp");
  _mp = num(*o, u"mp");
  _hp_map = KeyedMap(field_or(*o, u"hp_map"));
  _mp_map = KeyedMap(field_or(*o, u"mp_map"));
  _times = num(*o, u"times");
  _ratio = num(*o, u"ratio");
  _reserve = num(*o, u"reserve");
  _is_boss = optional_bool(*o, u"is_boss");
  _is_soldier = optional_bool(*o, u"is_soldier");
  _join = num(*o, u"join");
  _join_team = optional_text(*o, u"join_team");
  _join_reserve = num(*o, u"join_reserve");
  _outline_color = optional_text(*o, u"outline_color");
}

StagePhaseInfo::StagePhaseInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _title = optional_text(*o, u"title");
  _desc = text(*o, u"desc");
  _music = music_text(*o, u"music");
  _bound = num(*o, u"bound");
  _player_l = num(*o, u"player_l");
  _player_r = num(*o, u"player_r");
  _camera_l = num(*o, u"camera_l");
  _camera_r = num(*o, u"camera_r");
  _enemy_l = num(*o, u"enemy_l");
  _enemy_r = num(*o, u"enemy_r");
  _drink_l = num(*o, u"drink_l");
  _drink_r = num(*o, u"drink_r");
  _cam_jump_to_x = num(*o, u"cam_jump_to_x");
  _player_jump_to_x = num(*o, u"player_jump_to_x");
  _player_jump_to_z = num(*o, u"player_jump_to_z");
  _player_facing = num(*o, u"player_facing");
  _hide_stats = num(*o, u"hide_stats");
  _world_pause = num(*o, u"world_pause");
  _control_disabled = num(*o, u"control_disabled");
  _weapon_rain_disabled = num(*o, u"weapon_rain_disabled");
  _end_test = texts(*o, u"end_test");
  _on_start = texts(*o, u"on_start");
  _on_end = texts(*o, u"on_end");
  _respawn = KeyedMap(field_or(*o, u"respawn"));
  _respawn_r = KeyedMap(field_or(*o, u"respawn_r"));
  _respawn_x = KeyedMap(field_or(*o, u"respawn_x"));
  _health_up = KeyedMap(field_or(*o, u"health_up"));
  _mp_up = KeyedMap(field_or(*o, u"mp_up"));
  _dialogs.clear();
  if (const Array* const arr = as_array(field_or(*o, u"dialogs"))) {
    _dialogs.reserve(arr->size());
    for (const Value& v : arr->items()) _dialogs.push_back(DialogInfo(v));
  }
  if (const Array* const arr = as_array(field_or(*o, u"sounds"))) {
    _sounds.reserve(arr->size());
    for (const Value& v : arr->items()) _sounds.push_back(SoundPlayInfo(v));
  }
  if (const Array* const arr = as_array(field_or(*o, u"objects"))) {
    _objects.reserve(arr->size());
    for (const Value& v : arr->items()) _objects.push_back(StageObjectInfo(v));
  }
}

}
