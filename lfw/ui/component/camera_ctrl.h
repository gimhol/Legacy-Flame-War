#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class CameraCtrl : public UIComponent {
 public:
  static constexpr const char* TAG = "CameraCtrl";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  bool auto_ = true;
  std::function<std::vector<Entity*>()> candidates;

  Entity* staring() const { return _staring; }
  void set_staring(Entity* v);

  void update(double dt) override;
  void on_stop() override;

  void focus_lr(double direction);
  void focus_ud(double direction);

 private:
  Entity* _staring = nullptr;
};

}
}
