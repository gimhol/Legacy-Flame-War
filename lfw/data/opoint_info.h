#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IOpointInfo`（`src/LFW/defines/IOpointInfo.ts`）：生成子实体的描述。
 *
 * 生成参数（位置/速度/血量/间隔/扩散…）是具体值，`gen_*` 是源表达式串；
 * `oid` / `action` / `multi` 以及 cook 编译出来的 `__gen_*` 仍是 `Value`，
 * 由 `Entity::spawn` 与 `gen_or` 这类生成期逻辑消费。
 */
class Opoint {
 public:
  Opoint() = default;
  explicit Opoint(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& name() const { return _name; }
  double kind() const { return _kind; }
  double x() const { return _x; }
  double y() const { return _y; }
  double z() const { return _z; }
  double pos_type() const { return _pos_type; }
  double dvx() const { return _dvx; }
  double dvy() const { return _dvy; }
  double dvz() const { return _dvz; }
  double max_hp() const { return _max_hp; }
  double hp() const { return _hp; }
  double max_mp() const { return _max_mp; }
  double mp() const { return _mp; }
  double speedz() const { return _speedz; }
  double spreading() const { return _spreading; }
  double ghost() const { return _ghost; }
  double interval() const { return _interval; }
  double interval_mode() const { return _interval_mode; }
  double motionless() const { return _motionless; }
  double unimportant() const { return _unimportant; }
  double delay() const { return _delay; }
  double inherit_speed_x() const { return _inherit_speed_x; }
  double inherit_speed_y() const { return _inherit_speed_y; }
  double inherit_speed_z() const { return _inherit_speed_z; }
  const Value& oid() const { return _oid; }
  const Value& action() const { return _action; }
  const Value& multi() const { return _multi; }
  const Value& interval_id() const { return _interval_id; }
  const Value& indicator_info() const { return _indicator_info; }
  const std::u16string& gen_x() const { return _gen_x; }
  const std::u16string& gen_y() const { return _gen_y; }
  const std::u16string& gen_z() const { return _gen_z; }
  const std::u16string& gen_dvx() const { return _gen_dvx; }
  const std::u16string& gen_dvy() const { return _gen_dvy; }
  const std::u16string& gen_dvz() const { return _gen_dvz; }
  const std::u16string& gen_spread_x() const { return _gen_spread_x; }
  const std::u16string& gen_spread_y() const { return _gen_spread_y; }
  const std::u16string& gen_spread_z() const { return _gen_spread_z; }
  const Value& compiled_gen_x() const { return _compiled_gen_x; }
  const Value& compiled_gen_y() const { return _compiled_gen_y; }
  const Value& compiled_gen_z() const { return _compiled_gen_z; }
  const Value& compiled_gen_dvx() const { return _compiled_gen_dvx; }
  const Value& compiled_gen_dvy() const { return _compiled_gen_dvy; }
  const Value& compiled_gen_dvz() const { return _compiled_gen_dvz; }
  const Value& compiled_gen_spread_x() const { return _compiled_gen_spread_x; }
  const Value& compiled_gen_spread_y() const { return _compiled_gen_spread_y; }
  const Value& compiled_gen_spread_z() const { return _compiled_gen_spread_z; }

  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _name;
  double _kind = 0;
  double _x = 0;
  double _y = 0;
  double _z = 0;
  double _pos_type = 0;
  double _dvx = 0;
  double _dvy = 0;
  double _dvz = 0;
  double _max_hp = 0;
  double _hp = 0;
  double _max_mp = 0;
  double _mp = 0;
  double _speedz = 0;
  double _spreading = 0;
  double _ghost = 0;
  double _interval = 0;
  double _interval_mode = 0;
  double _motionless = 0;
  double _unimportant = 0;
  double _delay = 0;
  double _inherit_speed_x = 0;
  double _inherit_speed_y = 0;
  double _inherit_speed_z = 0;
  Value _oid;
  Value _action;
  Value _multi;
  Value _interval_id;
  Value _indicator_info;
  std::u16string _gen_x;
  std::u16string _gen_y;
  std::u16string _gen_z;
  std::u16string _gen_dvx;
  std::u16string _gen_dvy;
  std::u16string _gen_dvz;
  std::u16string _gen_spread_x;
  std::u16string _gen_spread_y;
  std::u16string _gen_spread_z;
  Value _compiled_gen_x;
  Value _compiled_gen_y;
  Value _compiled_gen_z;
  Value _compiled_gen_dvx;
  Value _compiled_gen_dvy;
  Value _compiled_gen_dvz;
  Value _compiled_gen_spread_x;
  Value _compiled_gen_spread_y;
  Value _compiled_gen_spread_z;
};

}