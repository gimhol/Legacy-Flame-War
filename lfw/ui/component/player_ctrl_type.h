#pragma once

#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

class PlayerInfo;

namespace ui {

class PlayerCtrlType : public UIComponent {
 public:
  static constexpr const char* TAG = "PlayerCtrlType";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::u16string player_id();
  PlayerInfo* player();

  void on_resume() override;
  void on_pause() override;
  void on_click(LFWPointerEvent& e) override;
  void on_ctrl_changed(const Value& c);

 private:
  Callbacks::Remover _remover;
};

}
}
