#pragma once

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/keyed_map.h"

namespace lfw {
namespace director {

bool supports_platform(const Value& director, std::u16string_view platform);

/**
 * `IDirectorStageData`（`src/LFW/defines/IDirectorData.ts` 里的 `IDirectorStageData`）。
 *
 * 原数据允许出现任意键（`setup_scene` 会逐个 `assign_number`），
 * 所以底层仍保留整张表；具名访问器只是给已知的 17 个字段用。
 */
class DirectorStageData {
 public:
  DirectorStageData() = default;
  explicit DirectorStageData(Value raw) : _values(std::move(raw)) {}

  bool valid() const { return _values.valid(); }

  const KeyedMap& values() const { return _values; }
  double left() const { return _values.num(u"left"); }
  double right() const { return _values.num(u"right"); }
  double near_plane() const { return _values.num(u"near"); }
  double far_plane() const { return _values.num(u"far"); }
  double width() const { return _values.num(u"width"); }
  double depth() const { return _values.num(u"depth"); }
  double player_l() const { return _values.num(u"player_l"); }
  double player_r() const { return _values.num(u"player_r"); }
  double cam_l() const { return _values.num(u"cam_l"); }
  double cam_r() const { return _values.num(u"cam_r"); }
  double enemy_l() const { return _values.num(u"enemy_l"); }
  double enemy_r() const { return _values.num(u"enemy_r"); }
  double drink_l() const { return _values.num(u"drink_l"); }
  double drink_r() const { return _values.num(u"drink_r"); }
  double phase_time() const { return _values.num(u"phase_time"); }
  double dialog_time() const { return _values.num(u"dialog_time"); }

  const Value& to_value() const { return _values.to_value(); }

 private:
  KeyedMap _values;
};

/**
 * `ILayoutBaseData` / `IPointLayoutData` / `ILineLayoutData` / `ICircleLayoutData`。
 *
 * `x/y/z/length/radius/rx/rz` 是 `TNumberExpr`（数字或表达式字符串），
 * 由 `utils::eval_number` 求值，所以原样保留 `Value`（缺失是 `undefined`）。
 */
class ActorLayoutData {
 public:
  ActorLayoutData() = default;
  explicit ActorLayoutData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& type() const { return _type; }
  const std::u16string& axis() const { return _axis; }
  const Value& x() const { return _x; }
  const Value& y() const { return _y; }
  const Value& z() const { return _z; }
  const Value& length() const { return _length; }
  const Value& radius() const { return _radius; }
  const Value& rx() const { return _rx; }
  const Value& rz() const { return _rz; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _type;
  std::u16string _axis;
  Value _x;
  Value _y;
  Value _z;
  Value _length;
  Value _radius;
  Value _rx;
  Value _rz;
};

/** `IActorData`：导演要刷的一类角色。 */
class ActorData {
 public:
  ActorData() = default;
  explicit ActorData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::vector<std::u16string>& oid() const { return _oid; }
  const std::u16string& source() const { return _source; }
  const std::u16string& half() const { return _half; }
  /** 没写是 NaN。 */
  double count() const { return _count; }
  const ActorLayoutData& layout() const { return _layout; }
  const Value& at() const { return _at; }
  const Value& x() const { return _x; }
  const Value& y() const { return _y; }
  const Value& z() const { return _z; }
  const Value& dx() const { return _dx; }
  const Value& dy() const { return _dy; }
  const Value& dz() const { return _dz; }
  const KeyedMap& values() const { return _values; }
  const std::vector<std::u16string>& group() const { return _group; }
  const std::u16string& ctrl() const { return _ctrl; }
  const std::u16string& player_id() const { return _player_id; }
  /** 只有显式 `false` 才不 attach（TS 是 `attach !== false`）。 */
  bool attach() const { return _attach.value_or(true); }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::vector<std::u16string> _oid;
  std::u16string _source;
  std::u16string _half;
  double _count = std::numeric_limits<double>::quiet_NaN();
  ActorLayoutData _layout;
  Value _at;
  Value _x;
  Value _y;
  Value _z;
  Value _dx;
  Value _dy;
  Value _dz;
  KeyedMap _values;
  std::vector<std::u16string> _group;
  std::u16string _ctrl;
  std::u16string _player_id;
  std::optional<bool> _attach;
};

/**
 * `IDirectorData`（`src/LFW/defines/IDirectorData.ts`）。
 *
 * `setup` / `auto` / `timeline` 里的动作还是 `Value`：它们是
 * `TActionData` 的联合体、还带 `TNumberExpr`，交给 `run_action` 按 `op` 分派、
 * 由表达式求值器处理，硬拆成 struct 反而会把动态性丢掉。
 */
class DirectorData {
 public:
  DirectorData() = default;
  explicit DirectorData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& name() const { return _name; }
  const std::optional<std::u16string>& bg() const { return _bg; }
  const DirectorStageData& stage() const { return _stage; }
  const std::vector<ActorData>& actors() const { return _actors; }
  const KeyedMap& groups() const { return _groups; }
  const Value& setup() const { return _setup; }
  const Value& auto_rules() const { return _auto; }
  const Value& timeline() const { return _timeline; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _name;
  std::optional<std::u16string> _bg;
  DirectorStageData _stage;
  std::vector<ActorData> _actors;
  KeyedMap _groups;
  Value _setup;
  Value _auto;
  Value _timeline;
};

/**
 * `IDirectorGroupData`：`directors/<NN>_<name>.director.xml` 一项（含多个导演）。
 */
class DirectorGroupData { public:
  DirectorGroupData() = default;
  explicit DirectorGroupData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::optional<std::u16string>& name() const { return _name; }
  const std::u16string& file() const { return _file; }
  const std::vector<DirectorData>& directors() const { return _directors; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::optional<std::u16string> _name;
  std::u16string _file;
  std::vector<DirectorData> _directors;
};

}
}
