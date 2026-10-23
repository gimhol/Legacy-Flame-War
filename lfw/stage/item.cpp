#include "lfw/stage/item.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "lfw/defines/defines_data.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/team_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/stage/stage.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/js_add.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/type_check.h"

namespace lfw {
namespace stage {

Item::Item(Stage* stage, const StagePhaseInfo& phase, const StageObjectInfo& info)
    : _stage(stage), _phase(&phase), _info(&info), _end_delay(0.0, 120.0) {
  times = !std::isnan(info.times()) && truthy(Value(info.times()))
              ? std::optional<double>(round(info.times()))
              : std::nullopt;

  std::vector<Value> data_list;
  std::vector<std::shared_ptr<Randoming>> randoming_list;
  for (const std::u16string& oid : info.id()) {
    const Value data_v = _stage->datas_find(Value(oid));
    _is_fighter = _is_fighter || entity::is_fighter_data(data_v);
    if (truthy(data_v)) {
      data_list.push_back(data_v);
      continue;
    }
    const std::shared_ptr<Randoming> rd = _stage->datas_randoming_by_group(Value(oid));
    if (rd->src().empty()) continue;
    for (const Value& item : rd->src()) {
      if (entity::is_fighter_data(item)) {
        _is_fighter = true;
        break;
      }
    }
    randoming_list.push_back(rd);
  }
  if (!data_list.empty() || !randoming_list.empty()) {
    if (data_list.size() == 1 && randoming_list.empty()) {
      data = data_list[0];
    } else if (!data_list.empty()) {
      randoming_list.push_back(
          Randoming::create(u"stage_item_oid_randoming", data_list, _stage->mt()));
    }
  }

  if (!randoming_list.empty()) {
    randoming = RandomingOfItems::create(u"stage_item_oids_randoming", randoming_list,
                                        _stage->mt());
  }
}

Item::~Item() {
  for (const Watch& w : _watches) {
    if (w.off_team) w.off_team();
    if (w.off_dead) w.off_dead();
  }
  _watches.clear();
  _objects.clear();
}

void Item::forget(Entity* e) {
  for (size_t i = 0; i < _objects.size(); ++i) {
    if (_objects[i] == e) {
      _objects.erase(_objects.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  for (size_t i = 0; i < _watches.size(); ++i) {
    if (_watches[i].e != e) continue;
    const Watch w = _watches[i];
    _watches.erase(_watches.begin() + static_cast<std::ptrdiff_t>(i));
    if (w.off_team) w.off_team();
    if (w.off_dead) w.off_dead();
    break;
  }
}

void Item::update() {
  if (_released) return;

  if (!_objects.empty()) {
    _end_delay.reset();
    return;
  }
  if (!_end_delay.add()) return;
  const double times_v = times.has_value() ? *times : -1.0;
  if (_info->is_soldier()) {
    if (_stage->all_boss_dead() || times_v == 0.0) {
      release();
      return;
    }
    spawn();
  } else if (times_v >= 1.0) {
    spawn();
  } else {
    release();
  }
}

bool Item::spawn() {
  Value data_v;
  if (truthy(data)) {
    data_v = data;
  } else if (randoming) {
    const std::shared_ptr<Randoming> picked = randoming->get();
    data_v = picked ? picked->get() : Value();
  }
  if (!truthy(data_v)) return false;

  Entity* const e = _stage->create_entity_with_bot(data_v);
  if (e == nullptr) return false;

  const auto or_default = [](double v, double fallback) {
    return std::isnan(v) ? fallback : v;
  };
  const double enemy_l = or_default(_phase->enemy_l(), 0.0);
  const double enemy_r = or_default(_phase->enemy_r(), 0.0);
  const Value difficulty = _stage->difficulty();
  const std::u16string difficulty_key = to_string(difficulty);

  const double x = std::isnan(_info->x()) ? _stage->mt()->next_float() < 0.5 ? enemy_l : enemy_r
                                          : _info->x();
  const double y = _info->y();
  const double z = _info->z();
  const double reserve = _info->reserve();
  const double hp = _info->hp();
  const KeyedMap& hp_map = _info->hp_map();
  const KeyedMap& mp_map = _info->mp_map();
  Value mp = Value(_info->mp());
  if (std::isnan(_info->mp())) {
    mp = mp_map.valid() ? mp_map.get(difficulty_key) : Value();
  }
  const std::optional<std::u16string>& outline_color = _info->outline_color();

  if (times.has_value() && truthy(Value(*times))) *times = *times - 1.0;

  e->set_outline_color_raw(outline_color.has_value() ? Value(*outline_color)
                                                     : Value(std::u16string()));
  if (entity::is_fighter(*e)) {
    e->set_outline_color_raw(outline_color.has_value() ? Value(*outline_color)
                                                       : Value(std::u16string(u"#FF0000")));
    e->set_stat_bar(0.0);
    e->set_wakeup_invuln(Value(0.0));
  }
  e->set_dead_gone(1.0);
  e->set_reserve(or_default(reserve, 0.0));

  _stage->mt()->mark = u"stage_item_spawn";

  const double range_x = or_default(_info->range_x(), 200.0);
  const double range_y = or_default(_info->range_y(), 0.0);
  const double range_z = or_default(_info->range_z(), 0.0);

  const double min_x = x;
  const double max_x = x + range_x;
  const bool z_is_num = !std::isnan(z);
  const double min_z = z_is_num ? z : _stage->far_plane();
  const double max_z = z_is_num ? z + range_z : _stage->near_plane();
  const bool y_is_num = !std::isnan(y);
  const double y_default = entity::is_weapon(*e) ? 300.0 : 0.0;
  const double min_y = y_is_num ? y : y_default;
  const double max_y = y_is_num ? y + range_y : y_default;

  const double px = _stage->mt()->range(min_x, max_x);
  const double py = _stage->mt()->range(min_y, max_y);
  const double pz = _stage->mt()->range(min_z, max_z);

  e->set_position(Value(px), Value(py), Value(pz));

  const double join = _info->join();
  if (truthy(Value(join))) {
    auto dead_join = std::make_shared<Object>();
    dead_join->set(u"hp", Value(join));
    dead_join->set(u"team", _info->join_team().has_value()
                                ? Value(*_info->join_team())
                                : Value(std::u16string(team_enum::kTeam_1)));
    dead_join->set(u"reserve", std::isnan(_info->join_reserve()) ? Value()
                                                                 : Value(_info->join_reserve()));
    e->set_dead_join(Value(dead_join));
  }

  Value hp_value;
  if (hp_map.valid()) hp_value = hp_map.get(difficulty_key);
  if (!is_num(hp_value) && !std::isnan(hp)) {
    if (strict_equals(difficulty, Value(static_cast<double>(Difficulty::Easy)))) {
      hp_value = Value(round(hp * 3.0 / 4.0));
    } else if (strict_equals(difficulty, Value(static_cast<double>(Difficulty::Crazy)))) {
      hp_value = Value(round(hp * 3.0 / 2.0));
    } else {
      hp_value = Value(hp);
    }
  }
  if (is_num(hp_value)) {
    e->set_hp_max(to_number(hp_value));
    e->set_hp_r(to_number(hp_value));
    e->set_hp(to_number(hp_value));
  }
  if (is_num(mp)) {
    e->set_mp_max(to_number(mp));
    e->set_mp(to_number(mp));
  }

  if (entity::is_fighter(*e)) {
    e->set_name(field_or(e->data()->base(), u"name"));
  }

  e->set_team(to_string(_stage->team()));
  e->attach();

  const double facing = _info->facing();
  if (facing == 1.0 || facing == -1.0) e->set_facing(Value(facing));
  if (_info->act().has_value()) {
    e->enter_frame_by_id(Value(*_info->act()));
  } else if (entity::is_fighter(*e)) {
    e->enter_frame_by_id(Value(std::u16string(u"running_0")));
  } else {
    const Value* const auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
    e->enter_frame(auto_frame != nullptr ? *auto_frame : Value());
  }

  Watch w;
  w.e = e;
  w.off_team = e->callbacks.on(u"on_team_changed",
                                 [this, e](const Callbacks::Payloads&) { forget(e); });
  w.off_dead =
      e->callbacks.on(u"on_dead", [this, e](const Callbacks::Payloads&) { forget(e); });
  _watches.push_back(std::move(w));

  for (Entity* const cur : _objects) {
    if (cur == e) return true;
  }
  _objects.push_back(e);
  return true;
}

void Item::release() { _released = true; }

}
}
