#include "lfw/entity/summary_mgr.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/defines/team_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/entity/summary.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/js_add.h"

namespace lfw {
namespace {

}

std::shared_ptr<Summary>* SummaryMgr::find_item(std::u16string_view id) {
  const auto it = _index.find(id);
  if (it != _index.end()) return &_items[it->second].second;
  return nullptr;
}

void SummaryMgr::reindex() {
  _index.clear();
  _index.reserve(_items.size());
  for (std::size_t i = 0; i < _items.size(); ++i) _index.emplace(_items[i].first, i);
}

std::shared_ptr<Summary> SummaryMgr::get(const std::u16string& id) {
  std::shared_ptr<Summary>* found = find_item(id);
  if (found != nullptr) return *found;
  std::shared_ptr<Summary> made = acquire(id);
  _items.push_back(std::make_pair(id, made));
  _index.emplace(_items.back().first, _items.size() - 1);
  return made;
}

void SummaryMgr::clear() {
  for (auto& kv : _items) {
    kv.second->release();
    _graves.push_back(std::move(kv.second));
  }
  _items.clear();
  _index.clear();
}

void SummaryMgr::release(const std::u16string& id) {
  std::shared_ptr<Summary>* found = find_item(id);
  if (found == nullptr) return;
  std::shared_ptr<Summary> item = *found;
  item->release();
  std::size_t pos = _index.find(id)->second;
  _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(pos));
  reindex();
  _graves.push_back(item);
}

std::shared_ptr<Summary> SummaryMgr::acquire(const std::u16string& id) {
  if (!_graves.empty()) {
    std::shared_ptr<Summary> ret = _graves.back();
    _graves.pop_back();
    ret->reset(id);
    return ret;
  }
  return std::make_shared<Summary>(id);
}

void SummaryMgr::add_damage_sum(const Entity& a, const Value& value) {
  std::shared_ptr<Summary> self = get(a.id);
  self->set_damage_sum(js_add(self->damage_sum(), value));
  const std::u16string& team = a.team();
  if (is_independent(team)) {
    std::shared_ptr<Summary> team_sum = get(team);
    team_sum->set_damage_sum(js_add(team_sum->damage_sum(), value));
  }
}

void SummaryMgr::add_kill_sum(const Entity& a, const Value& value) {
  const Value amount = std::holds_alternative<std::monostate>(value) ? Value(1.0) : value;
  std::shared_ptr<Summary> self = get(a.id);
  self->set_kill_sum(js_add(self->kill_sum(), amount));
  const std::u16string& team = a.team();
  if (is_independent(team)) {
    std::shared_ptr<Summary> team_sum = get(team);
    team_sum->set_kill_sum(js_add(team_sum->kill_sum(), amount));
  }
}

void SummaryMgr::apply_damage(const Entity& a, const Value& injury, const Entity& v,
                              const Value& prev_hp) {
  add_damage_sum(a, injury);
  if (entity::is_fighter(v) && v.emitters.empty() && le(Value(v.hp()), Value(0.0)) &&
      gt(prev_hp, Value(0.0))) {
    summary_mgr().add_kill_sum(a);
  }
}

SummaryMgr& summary_mgr() {
  static SummaryMgr mgr;
  return mgr;
}

}
