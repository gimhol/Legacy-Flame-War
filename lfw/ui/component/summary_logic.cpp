#include "lfw/ui/component/summary_logic.h"

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/controller/base_controller.h"
#include "lfw/defines/defines_data.h"
#include "lfw/entity/summary.h"
#include "lfw/entity/summary_mgr.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/times.h"
#include "lfw/world.h"

namespace lfw {
namespace ui {

namespace {

constexpr double kLosingWait = 180;

}

struct SummaryLogic::DPtr {
  using ValueListener = NoEmitCallbacksT<Value>::Listener;
  using WorldListener = NoEmitCallbacksT<WorldCallbackArgs>::Listener;
  using ValuePayloads = NoEmitCallbacksT<Value>::Payloads;
  using WorldPayloads = NoEmitCallbacksT<WorldCallbackArgs>::Payloads;

  struct WorldListenerT : WorldListener {
    explicit WorldListenerT(SummaryLogic* o);
  };
  struct FighterListenerT : ValueListener {
    explicit FighterListenerT(SummaryLogic* o);
  };
  struct SummaryListenerT : ValueListener {
    SummaryListenerT(SummaryLogic* o, Entity* e);
  };

  explicit DPtr(SummaryLogic& owner) : world_cb(&owner), fighter_cb(&owner), self(owner) {}

  WorldListenerT world_cb;
  FighterListenerT fighter_cb;
  std::map<std::u16string, std::unique_ptr<SummaryListenerT>> summary_cb_map;
  std::vector<std::unique_ptr<SummaryListenerT>> summary_cb_trash;
  Times refresh_timer{0.0, 300.0};
  std::vector<std::u16string> temps;
  SummaryLogic& self;
};

SumInfo make_sum_info(const std::u16string& team) {
  SumInfo ret;
  ret.team = team;
  return ret;
}

FighterSumInfo make_fighter_sum(const Value& data) {
  FighterSumInfo ret;
  ret.team = u"";
  ret.data = data;
  return ret;
}

PlayerSumInfo make_player_sum(Entity& e) {
  PlayerSumInfo ret;
  ret.team = u"";
  ret.data = e.data()->to_value();
  const controller::BaseController* const ctrl = e.ctrl();
  ret.player_id = ctrl != nullptr ? ctrl->player_id : std::u16string();
  return ret;
}

TeamSumInfo make_team_sum_info(const std::u16string& team) {
  TeamSumInfo ret;
  ret.team = team;
  return ret;
}

const ClazzTag* SummaryLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& SummaryLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"SummaryLogic"};
  return tags;
}

SummaryLogic::DPtr::WorldListenerT::WorldListenerT(SummaryLogic* o) {
  handlers.push_back({u"on_fighter_del", [o](const WorldPayloads& args) {
                        for (const WorldCallbackArgs& a : args) {
                          if (a.entity != nullptr) o->on_fighter_del(*a.entity);
                        }
                      }});
  handlers.push_back({u"on_fighter_add", [o](const WorldPayloads& args) {
                        for (const WorldCallbackArgs& a : args) {
                          if (a.entity != nullptr) o->on_fighter_add(*a.entity);
                        }
                      }});
}

SummaryLogic::DPtr::FighterListenerT::FighterListenerT(SummaryLogic* o) {
  handlers.push_back({u"on_dead", [o](const ValuePayloads& a) {
                        if (!a.empty()) o->handle_dead(a[0]);
                      }});
  handlers.push_back({u"on_hp_changed", [o](const ValuePayloads& a) {
                        if (a.size() >= 3) o->handle_hp_changed(a[0], a[1], a[2]);
                      }});
  handlers.push_back({u"on_mp_changed", [o](const ValuePayloads& a) {
                        if (a.size() >= 3) o->handle_mp_changed(a[0], a[1], a[2]);
                      }});
  handlers.push_back({u"on_reserve_changed", [o](const ValuePayloads& a) {
                        if (a.size() >= 3) o->handle_reserve_changed(a[0], a[1], a[2]);
                      }});
}

SummaryLogic::DPtr::SummaryListenerT::SummaryListenerT(SummaryLogic* o, Entity* e) {
  handlers.push_back({u"on_damage_sum_changed", [o, e](const ValuePayloads& a) {
                        if (a.size() >= 2) o->handle_damage_sum_changed(*e, a[0], a[1]);
                      }});
  handlers.push_back({u"on_kill_sum_changed", [o, e](const ValuePayloads& a) {
                        if (a.size() >= 2) o->handle_kill_sum_changed(*e, a[0], a[1]);
                      }});
  handlers.push_back({u"on_picking_sum_changed", [o, e](const ValuePayloads& a) {
                        if (a.size() >= 2) o->handle_picking_sum_changed(*e, a[0], a[1]);
                      }});
}

SummaryLogic::SummaryLogic(UINode& layout, const std::u16string& f_name, const Value& info)
    : UIComponent(layout, f_name, info), dptr(std::make_unique<DPtr>(*this)) {}

SummaryLogic::~SummaryLogic() = default;

size_t SummaryLogic::cb_map_count() const { return dptr->summary_cb_map.size(); }

std::u16string SummaryLogic::data_id_of(const Entity& e) { return e.data()->id(); }

TeamSumInfo& SummaryLogic::team_sum(const std::u16string& team) {
  for (std::pair<std::u16string, TeamSumInfo>& kv : teams) {
    if (kv.first == team) return kv.second;
  }
  teams.push_back({team, make_team_sum_info(team)});
  return teams.back().second;
}

FighterSumInfo& SummaryLogic::fighter_sum(Entity& e) {
  const std::u16string key = data_id_of(e);
  for (std::pair<std::u16string, FighterSumInfo>& kv : fighters) {
    if (kv.first == key) return kv.second;
  }
  fighters.push_back({key, make_fighter_sum(e.data()->to_value())});
  return fighters.back().second;
}

PlayerSumInfo& SummaryLogic::player_sum(Entity& e) {
  const controller::BaseController* const ctrl = e.ctrl();
  const std::u16string key = ctrl != nullptr ? ctrl->player_id : std::u16string();
  for (std::pair<std::u16string, PlayerSumInfo>& kv : players) {
    if (kv.first == key) return kv.second;
  }
  players.push_back({key, make_player_sum(e)});
  return players.back().second;
}

bool SummaryLogic::is_losing(const std::u16string& team) const {
  for (const std::u16string& v : losing_teams) {
    if (v == team) return true;
  }
  return false;
}

void SummaryLogic::add_losing(const std::u16string& team) {
  if (!is_losing(team)) losing_teams.push_back(team);
}

void SummaryLogic::del_losing(const std::u16string& team) {
  for (size_t i = 0; i < losing_teams.size(); ++i) {
    if (losing_teams[i] == team) {
      losing_teams.erase(losing_teams.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
}

Entity* SummaryLogic::entity_of(const Value& ref) {
  const Value* const id_v = field_of(ref, u"id");
  if (id_v == nullptr) return nullptr;
  return world().find_entity(to_string(*id_v));
}

void SummaryLogic::handle_dead(const Value& ref) {
  Entity* const e = entity_of(ref);
  if (e == nullptr) return;
  TeamSumInfo& team_sum_ = team_sum(e->team());
  team_sum_.deads++;
  team_sum_.lives--;
  team_sum_.latest_dead_time = node.lifetime();
  if (team_sum_.deads == team_sum_.spawns) add_losing(e->team());
  fighter_sum(*e).deads++;
  player_sum(*e).deads++;
}

void SummaryLogic::handle_hp_changed(const Value& ref, const Value& value, const Value& prev) {
  Entity* const e = entity_of(ref);
  if (e == nullptr) return;
  const double v = to_number(value);
  const double p = to_number(prev);
  TeamSumInfo& team_sum_ = team_sum(e->team());
  team_sum_.hp += max(v, 0.0) - p;
  if (p <= 0 && v > 0) {
    team_sum_.deads--;
    fighter_sum(*e).deads--;
    player_sum(*e).deads--;
    team_sum_.lives++;
  } else if (p > v) {
    const double d = p - v;
    team_sum_.hp_lost += d;
    fighter_sum(*e).hp_lost += d;
    player_sum(*e).hp_lost += d;
  }
}

void SummaryLogic::handle_mp_changed(const Value& ref, const Value& value, const Value& prev) {
  Entity* const e = entity_of(ref);
  if (e == nullptr) return;
  const double v = to_number(value);
  const double p = to_number(prev);
  if (p > v) {
    const double d = p - v;
    team_sum(e->team()).mp_usage += d;
    fighter_sum(*e).mp_usage += d;
    player_sum(*e).mp_usage += d;
  }
}

void SummaryLogic::handle_reserve_changed(const Value& ref, const Value& value,
                                          const Value& prev) {
  Entity* const e = entity_of(ref);
  if (e == nullptr) return;
  team_sum(e->team()).reserve += to_number(value) - to_number(prev);
}

void SummaryLogic::handle_damage_sum_changed(Entity& e, const Value& value, const Value& prev) {
  const double d = to_number(value) - to_number(prev);
  team_sum(e.team()).damages += d;
  fighter_sum(e).damages += d;
  player_sum(e).damages += d;
}

void SummaryLogic::handle_kill_sum_changed(Entity& e, const Value& value, const Value& prev) {
  const double d = to_number(value) - to_number(prev);
  team_sum(e.team()).kills += d;
  fighter_sum(e).kills += d;
  player_sum(e).kills += d;
}

void SummaryLogic::handle_picking_sum_changed(Entity& e, const Value& value, const Value& prev) {
  const double d = to_number(value) - to_number(prev);
  team_sum(e.team()).pickings += d;
  fighter_sum(e).pickings += d;
  player_sum(e).pickings += d;
}

void SummaryLogic::on_fighter_add(Entity& e) {
  TeamSumInfo& team_sum_ = team_sum(e.team());
  team_sum_.hp += e.hp();
  team_sum_.reserve += e.reserve();
  team_sum_.lives++;
  e.callbacks.add(&dptr->fighter_cb);

  if (e.emitters.empty()) {
    team_sum_.spawns++;
    del_losing(e.team());
    fighter_sum(e).spawns++;
    player_sum(e).spawns++;
    const std::map<std::u16string, std::unique_ptr<DPtr::SummaryListenerT>>::iterator old =
        dptr->summary_cb_map.find(e.id);
    if (old != dptr->summary_cb_map.end()) dptr->summary_cb_trash.push_back(std::move(old->second));
    auto cbs = std::make_unique<DPtr::SummaryListenerT>(this, &e);
    summary_mgr().get(e.id)->callbacks.add(cbs.get());
    dptr->summary_cb_map[e.id] = std::move(cbs);
  }
}

void SummaryLogic::on_fighter_del(Entity& e) {
  e.callbacks.del(&dptr->fighter_cb);
  const std::map<std::u16string, std::unique_ptr<DPtr::SummaryListenerT>>::iterator it =
      dptr->summary_cb_map.find(e.id);
  if (it != dptr->summary_cb_map.end()) {
    summary_mgr().get(e.id)->callbacks.del(it->second.get());
    dptr->summary_cb_map.erase(it);
  }
}

void SummaryLogic::on_start() {
  for (Entity* const e : world().entities) {
    if (entity::is_fighter(*e)) on_fighter_add(*e);
  }
  world().callbacks.add(&dptr->world_cb);
}

void SummaryLogic::on_stop() { world().callbacks.del(&dptr->world_cb); }

void SummaryLogic::update(double dt) {
  (void)dt;
  if (!losing_teams.empty()) {
    for (const std::u16string losing_team : std::vector<std::u16string>(losing_teams)) {
      TeamSumInfo& sum = team_sum(losing_team);
      const bool is_waiting = node.lifetime() - sum.latest_dead_time < kLosingWait;
      if (is_waiting) continue;
      sum.loses++;
      dptr->temps.push_back(losing_team);
    }
  }
  if (!dptr->temps.empty()) {
    for (const std::u16string& t : dptr->temps) del_losing(t);
    dptr->temps.clear();
  }

  if (dptr->refresh_timer.add()) {
    const size_t team_count = teams.size();
    for (size_t i = 0; i < team_count; ++i) {
      teams[i].second.reserve = 0;
      teams[i].second.lives = 0;
      teams[i].second.hp = 0;
    }
    for (Entity* const e : world().entities) {
      if (!entity::is_fighter(*e)) continue;
      TeamSumInfo& t = team_sum(e->team());
      t.hp += e->hp();
      t.reserve += e->reserve();
      if (e->hp() > 0) t.lives += 1;
    }
  }
}

}
}
