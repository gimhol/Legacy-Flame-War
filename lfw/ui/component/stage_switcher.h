#pragma once

#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class StageSwitcher : public UIComponent {
 public:
  static constexpr const char* TAG = "StageSwitcher";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  bool show_all();
  std::vector<Value> stages();
  const Value& stage() const { return _stage; }
  std::u16string stage_text();

  void on_resume() override;
  void on_pause() override;
  void on_show() override;

  void on_broadcast(const std::u16string& v);

  static Value default_stage();
  Value _stage = default_stage();
  LfwCallbacks::Remover _remover;
};

}
}
