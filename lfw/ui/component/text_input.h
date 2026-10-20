#pragma once

#include <optional>
#include <string>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class TextInput : public UIComponent {
 public:
  static constexpr const char* TAG = "TextInput";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  virtual std::optional<double> max_length();
  virtual std::optional<std::u16string> default_value();

  const std::u16string& text() const { return _text; }
  void set_text(const std::u16string& v) { _text = v; }

 protected:
  std::u16string _text;
};

}
}
