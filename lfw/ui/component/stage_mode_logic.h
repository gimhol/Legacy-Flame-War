#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

namespace ui {

class ComponentsPlayer;
class FighterStatBar;
class Jalousie;

class StageModeLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "StageModeLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  StageModeLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~StageModeLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  FSM* fsm() override { return &_fsm; }

  void handle_stage_actions(const Value& actions);
  void on_start() override;
  void on_stop() override;
  void update(double dt) override;
  void on_key_down(LFWKeyEvent& e) override;

  Jalousie* jalousie() const;
  ComponentsPlayer* gogogo() const;
  ComponentsPlayer* gogogo_loop() const;
  UINode* rank_hint_cheat_node() const;
  UINode* rank_hint_mods_node() const;

  FSM _fsm;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
