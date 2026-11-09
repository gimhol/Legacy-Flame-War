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
  /**
   * 按钮自带的两个文字节点
   *
   * back_label 是常态文字，hover_label 叠在上面、靠 opacity 在悬停时淡入。
   * 由组件自己声明，UI 文件只需给 text / style_normal / style_hover / style_disabled 这些 values。
   * 想换用别的节点时，仍可按老办法在 properties 里指定 back_label / hover_label（指定了就不再自动创建）。
   */
  static const Value& ITEMS();

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
  /** 优先用 props 指定的节点，否则用 ITEMS 自动创建的同名节点 */
  UINode* label_node(const std::u16string& prop, const std::u16string& id);

  Value _checked = Value(NullTag{});
};

}
}
