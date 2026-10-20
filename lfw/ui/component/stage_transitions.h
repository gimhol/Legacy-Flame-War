#pragma once

#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class StageTransitions : public UIComponent {
 public:
  static constexpr const char* TAG = "StageTransitions";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_resume() override;
  void on_pause() override;
  void on_enter_next_stage() {}

 private:
  LfwCallbacks::Remover _remover;
};

}
}
