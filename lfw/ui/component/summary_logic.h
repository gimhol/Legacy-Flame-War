#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/entity/entity.h"
#include "lfw/ui/component/sum_info.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/utils/times.h"
#include "lfw/world.h"

namespace lfw {
namespace ui {

class SummaryLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "SummaryLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  explicit SummaryLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~SummaryLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::vector<std::pair<std::u16string, FighterSumInfo>> fighters;
  std::vector<std::pair<std::u16string, PlayerSumInfo>> players;
  std::vector<std::pair<std::u16string, TeamSumInfo>> teams;
  std::vector<std::u16string> losing_teams;

  TeamSumInfo& team_sum(const std::u16string& team);
  FighterSumInfo& fighter_sum(Entity& e);
  PlayerSumInfo& player_sum(Entity& e);

  bool is_losing(const std::u16string& team) const;
  void add_losing(const std::u16string& team);
  void del_losing(const std::u16string& team);

  size_t cb_map_count() const;

  virtual void on_fighter_add(Entity& e);
  virtual void on_fighter_del(Entity& e);

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;

  void handle_dead(const Value& ref);
  void handle_hp_changed(const Value& ref, const Value& value, const Value& prev);
  void handle_mp_changed(const Value& ref, const Value& value, const Value& prev);
  void handle_reserve_changed(const Value& ref, const Value& value, const Value& prev);
  void handle_damage_sum_changed(Entity& e, const Value& value, const Value& prev);
  void handle_kill_sum_changed(Entity& e, const Value& value, const Value& prev);
  void handle_picking_sum_changed(Entity& e, const Value& value, const Value& prev);

  Entity* entity_of(const Value& ref);
  static std::u16string data_id_of(const Entity& e);

 protected:
  using ValuePayloads = NoEmitCallbacksT<Value>::Payloads;
  using WorldPayloads = NoEmitCallbacksT<WorldCallbackArgs>::Payloads;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
