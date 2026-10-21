#pragma once

#include <string>

#include "lfw/ui/component/stage_dialog_listener.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class HideWhenDialoging : public UIComponent {
 public:
  static constexpr const char* TAG = "HideWhenDialoging";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  HideWhenDialoging(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;
  void set_dialog(const DialogInfo* d);

  StageDialogListener& dialog_listener() { return _dialog_listener; }

 protected:
  StageDialogListener _dialog_listener{
      *this, [this](const DialogInfo* d, const DialogInfo*) { set_dialog(d); }};
};

}
}
