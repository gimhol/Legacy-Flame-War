#pragma once

#include <string>

#include "lfw/transform.h"
#include "lfw/ui/component/stage_dialog_listener.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/ui/ui_img_loader.h"

namespace lfw {
namespace ui {

class UINode;

class Dialogs : public UIComponent {
 public:
  static constexpr const char* TAG = "Dialogs";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  Dialogs(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;

  UINode* head_node();
  UINode* text_node();
  UINode* talker_node();

  bool hiding() const { return _hiding; }
  Transform& transform() { return _transform; }

  void hide_dialog();
  void show_dialog(const DialogInfo& dialog);
  void set_dialog(const DialogInfo* dialog);

 protected:
  void set_head_img(const std::u16string& path);

  StageDialogListener _listner;
  UINodeImgAdapter _head_adapter;
  UIImgLoader _head_loader;
  Transform _transform;
  bool _hiding = false;
};

}
}
