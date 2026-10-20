#pragma once

#include <map>
#include <optional>
#include <string>

#include "lfw/helper/randoming.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class RandomVisible : public UIComponent {
 public:
  static constexpr const char* TAG = "RandomVisible";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::optional<std::u16string> group();
  Randoming& randoming();
  void on_resume() override;

 protected:
  static std::map<std::u16string, Randoming::Ptr>& by_group();
  static std::map<const RandomVisible*, Randoming::Ptr>& by_self();
};

}
}
