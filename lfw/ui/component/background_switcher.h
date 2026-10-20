#pragma once

#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class BackgroundSwitcher : public UIComponent {
 public:
  static constexpr const char* TAG = "BackgroundSwitcher";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::vector<Value> backgrounds();
  const Value& background() const { return _background; }

  void on_resume() override;
  void on_pause() override;
  void on_show() override;

  void on_broadcast(const std::u16string& v);

  static Value default_background();
  Value _background = default_background();
  LfwCallbacks::Remover _remover;
};

}
}
