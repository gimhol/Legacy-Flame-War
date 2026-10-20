#pragma once

#include "lfw/animation/easing.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class OpacityHover : public UIComponent {
 public:
  static constexpr const char* TAG = "OpacityHover";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  OpacityHover(UINode& layout, const std::u16string& f_name, const Value& info)
      : UIComponent(layout, f_name, info) {
    anim.set_duration(150);
  }

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void update(double dt) override;

 protected:
  Easing anim{0, 1};

 private:
  std::optional<double> _p;
};

}
}
