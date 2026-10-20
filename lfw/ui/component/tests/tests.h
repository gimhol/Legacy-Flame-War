#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

class Entity;

namespace ui {

class TestCase;

class Tests : public UIComponent {
 public:
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  ~Tests() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  FSM* fsm() override { return &_fsm; }

  void init() override;
  void on_start() override;
  void on_stop() override;
  void on_key_down(LFWKeyEvent& e) override;
  void update(double dt) override;

  std::vector<std::vector<TestCase*>> groups;

 private:
  FSM _fsm;
  std::vector<TestCase*> _owned;
  std::vector<TestCase*> _retired;
};

}
}
