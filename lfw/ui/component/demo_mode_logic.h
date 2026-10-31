#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

namespace ui {

class CameraCtrl;
class ComponentsPlayer;
class Jalousie;

class DemoModeLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "DemoModeLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();
  static const Value& PROPS();

  DemoModeLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~DemoModeLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  const Value& props_meta() const override { return PROPS(); }
  FSM* fsm() override { return &_fsm; }

  UINode* focus_prefix();
  UINode* focus_on();
  CameraCtrl* cam_ctrl();
  UINode* score_board();
  UINode* situation_name();
  UINode* focus_text_node();
  Jalousie* jalousie();
  ComponentsPlayer* gogogo();
  ComponentsPlayer* gogogo_loop();

  bool is_stage_mode();
  bool is_vs_mode();

  void handle_stage_actions(const Value& actions);
  void startup();
  void clearup();

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;
  void on_key_down(LFWKeyEvent& e) override;

  FSM _fsm;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
