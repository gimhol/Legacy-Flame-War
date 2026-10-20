#pragma once

#include <string>
#include <vector>

#include "lfw/animation/loop.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class Sounds : public UIComponent {
 public:
  static constexpr const char* TAG = "Sounds";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  struct Step {
    double t = 0.0;
    std::u16string s;
    bool builtin = false;
  };

  std::vector<Step> seq;
  double idx = 0.0;
  double time = 0.0;
  Loop loop;

  void start() override;
  void stop() override;
  void replay() override;

  bool is_playable() const override { return true; }

  void on_start() override;
  void on_resume() override;
  void update(double dt) override;
};

}
}
