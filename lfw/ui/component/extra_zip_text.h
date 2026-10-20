#pragma once

#include <functional>
#include <string>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ExtraZipText : public UIComponent {
 public:
  static constexpr const char* TAG = "ExtraZipText";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;

  void render();

 protected:
  std::function<void()> _remove_zips;
  std::function<void()> _remove_lang;
};

}
}
