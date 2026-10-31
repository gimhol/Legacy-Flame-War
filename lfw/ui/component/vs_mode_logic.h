#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class VsModeLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "VsModeLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  VsModeLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~VsModeLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  FSM* fsm() override { return &_fsm; }

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
