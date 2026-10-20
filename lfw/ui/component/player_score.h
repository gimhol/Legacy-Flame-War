#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/entity/entity.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class PlayerScore : public UIComponent {
 public:
  static constexpr const char* TAG = "PlayerScore";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  double index();
  std::optional<bool> lose();
  Entity* fighter();

  void update(double dt) override;
};

}
}
