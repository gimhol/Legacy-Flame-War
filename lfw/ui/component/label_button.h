#pragma once

#include <string>
#include <vector>

#include "lfw/animation/easing.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class UINode;

class LabelButton : public UIComponent {
 public:
  static constexpr const char* TAG = "LabelButton";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  LabelButton(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  const Value& checked() const { return _checked; }
  void set_checked(const Value& v) { _checked = v; }

  UINode& responser();
  std::u16string normal_color();
  std::u16string hover_color();
  std::u16string text() const;
  void set_text(const std::u16string& v);

  void update(double dt) override;

  Easing anim{0, 1};

 private:
  UINode* node_prop(const std::u16string& name);

  Value _checked = Value(NullTag{});
};

}
}
