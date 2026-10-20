#pragma once

#include <functional>
#include <string>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/player_info.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class PlayerKeyText : public UIComponent {
 public:
  static constexpr const char* TAG = "PlayerKeyText";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_resume() override;
  void on_pause() override;

  virtual void on_key_changed();

  std::u16string player_id();
  std::u16string key_name();
  PlayerInfo* player();
  std::u16string key_code();

 protected:
  std::u16string pick(size_t idx, const char16_t* field_name);

  std::function<void()> _remover;
};

}
}
