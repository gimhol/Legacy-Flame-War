#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "lfw/animation/sequence.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/ui/component/ui_component_expression_builder.h"

namespace lfw {
namespace ui {

class FadeOutOpacity : public UIComponent {
 public:
  static constexpr const char* TAG = "FadeOutOpacity";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  static UIComponentExpressionBuilder expression(std::optional<double> duration = 1000.0,
                                                 std::optional<double> delay = 0.0);

  void on_start() override;
  void update(double dt) override;

  void start(std::optional<bool> reverse = std::nullopt);

  Sequence& anim() { return _anim; }

 protected:
  Sequence _anim;
  std::vector<std::unique_ptr<Animation>> _owned;
};

}
}
