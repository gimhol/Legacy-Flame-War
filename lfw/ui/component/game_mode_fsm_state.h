#pragma once

#include "lfw/base/fsm.h"
#include "lfw/ui/component/component_fsm_state.h"
#include "lfw/ui/component/mode_state.h"

namespace lfw {
namespace ui {

class GameModeFSMState : public ComponentFSMState {
 public:
  using ComponentFSMState::ComponentFSMState;

  Value key() const override { return Value(mode_state_str(ModeState::Invalid)); }
  FSM* fsm();
};

class GameModeFSMState_Running : public GameModeFSMState {
 public:
  using GameModeFSMState::GameModeFSMState;

  Value key() const override { return Value(mode_state_str(ModeState::Running)); }
  std::optional<Value> update(double dt) override;
};

class GameModeFSMState_BeforeEnd : public GameModeFSMState {
 public:
  using GameModeFSMState::GameModeFSMState;

  Value key() const override { return Value(mode_state_str(ModeState::BeforeEnd)); }
  void enter() override;
  void leave() override;
  std::optional<Value> update(double dt) override;

 private:
  double _time = 0.0;
};

class GameModeFSMState_End : public GameModeFSMState {
 public:
  using GameModeFSMState::GameModeFSMState;

  Value key() const override { return Value(mode_state_str(ModeState::End)); }
  void enter() override;
  void leave() override;
};

}
}
