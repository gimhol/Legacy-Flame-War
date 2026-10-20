#pragma once

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class FocusBehavior : public UIComponent {
 public:
  static constexpr const char* TAG = "FocusBehavior";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  std::u16string behavior();
  void on_foucs() override;
  void on_blur() override;
};

}
}
