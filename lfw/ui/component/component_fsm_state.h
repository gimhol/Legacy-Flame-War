#pragma once

#include "lfw/base/fsm.h"

namespace lfw {

class LFW;
class World;

namespace ui {

class UIComponent;
class UINode;

class ComponentFSMState : public IState {
 public:
  explicit ComponentFSMState(UIComponent& owner);

  UIComponent& owner;
  LFW& lfw;
  World& world;
  UINode& node;
};

}
}
