#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "lfw/ui/component/ui_component.h"
#include "lfw/utils/times.h"

namespace lfw {

class Entity;

namespace ui {

constexpr double kDefaultInterval = 1200;

class WeaponRain : public UIComponent {
 public:
  static constexpr const char* TAG = "WeaponRain";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  void on_start() override;
  void update(double dt) override;

  Times timer{0, kDefaultInterval};
  std::optional<double> last_section;

  const std::vector<const Value*>& oid_datas() const { return _oid_datas; }

 protected:
  void try_drop_weapon();
  Entity* drop_weapon(const std::u16string& groups);
  const std::vector<const Value*>& resolve_oid_datas();

  std::optional<std::u16string> _using_oids;
  std::vector<const Value*> _oid_datas;
  std::set<std::u16string> _warned_oids;
};

}
}
