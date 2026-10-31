#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ImgLoop;
class UINode;

class LaunchPageLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "LaunchPage";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  LaunchPageLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~LaunchPageLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  FSM* fsm() override { return &_fsm; }

  void on_start() override;
  void on_stop() override;
  void on_resume() override;
  void on_key_down(LFWKeyEvent& e) override;
  void on_click(LFWPointerEvent& e) override;
  void update(double dt) override;

  void on_tap();

  bool prel_loaded() const;
  ImgLoop* loading_anim();
  UINode* translation_note();
  UINode* tap_to_launch();
  UINode* sound_warning();
  UINode* yeonface();
  UINode* bearface();
  UINode* long_text();

  FSM _fsm;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
