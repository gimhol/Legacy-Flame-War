#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class Jalousie;

class EndingPageLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "EndingPageLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();
  static const Value& PROPS();

  EndingPageLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~EndingPageLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }
  const Value& props_meta() const override { return PROPS(); }

  void on_start() override;
  void on_resume() override;
  void on_stop() override;
  void on_click(LFWPointerEvent& e) override;
  void on_key_down(LFWKeyEvent& e) override;

  void on_jalousie_anim_end(Jalousie& j);

  UINode* label_node();
  Jalousie* jalousie();

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
