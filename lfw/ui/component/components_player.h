#pragma once

#include <string>
#include <vector>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ComponentsPlayer : public UIComponent {
 public:
  static constexpr const char* TAG = "ComponentsPlayer";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();
  static const Value& PROPS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  bool is_playable() const override { return true; }

  void on_start() override;
  void start() override;
  void stop() override;
  void replay() override;

  std::vector<UIComponent*> components;
};

}
}
