#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ReachableGroup;

class MainMenuLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "MainMenuLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_resume() override;

  void apply();

 private:
  std::vector<UINode*>& survival_btns();
  std::vector<UINode*>& danmu_btns();
  ReachableGroup* reachable_group();

  std::optional<std::vector<UINode*>> _survival_btns;
  std::optional<std::vector<UINode*>> _danmu_btns;
};

}
}
