#pragma once

#include <functional>

#include "lfw/core/value.h"
#include "lfw/stage/stage.h"

namespace lfw {
namespace ui {

class UIComponent;

class StageDialogListener {
 public:
  using Handler = std::function<void(const DialogInfo* dialog, const DialogInfo* prev)>;

  explicit StageDialogListener(UIComponent& owner, Handler handler = Handler());

  void start();
  void stop();
  StageDialogListener& set_handler(Handler handler);

  void set_stage(stage::Stage* stage);
  void set_dialog(const DialogInfo* dialog);

  UIComponent& owner() const { return *_owner; }
  stage::Stage* stage() const { return _stage; }

 protected:
  UIComponent* _owner = nullptr;
  Handler _handler;
  stage::Stage* _stage = nullptr;
  const DialogInfo* _dialog = nullptr;
  std::function<void()> _world_remover;
  std::function<void()> _stage_remover;
};

}
}
