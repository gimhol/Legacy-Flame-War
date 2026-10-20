#pragma once

#include <array>
#include <string>
#include <vector>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class Flex : public UIComponent {
 public:
  static constexpr const char* TAG = "Flex";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::u16string direction();
  double gap();
  double row_gap();
  double col_gap();
  std::u16string align();
  bool fit();
  bool fit_w();
  bool fit_h();
  std::array<double, 4> padding();

  void on_resume() override;
  void update(double dt) override;

  void apply();
};

}
}
