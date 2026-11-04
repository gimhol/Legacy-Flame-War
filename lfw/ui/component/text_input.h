#pragma once

#include <functional>
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
  virtual std::optional<double> font_size();
  virtual std::optional<std::u16string> font_family();

  const std::u16string& text() const { return _text; }
  void set_text(const std::u16string& v) { _text = v; }

  /** 提交 / 取消（Esc）回调，由使用方（如聊天输入）挂接。 */
  std::function<void()> on_submit;
  std::function<void()> on_cancel;

  /** 请求宿主把真实输入框聚焦 / 失焦（下一帧生效）。 */
  void request_focus() { _focus_request = true; }
  void request_blur() { _blur_request = true; }
  bool take_focus_request() {
    const bool v = _focus_request;
    _focus_request = false;
    return v;
  }
  bool take_blur_request() {
    const bool v = _blur_request;
    _blur_request = false;
    return v;
  }

 protected:
  std::u16string _text;
  bool _focus_request = false;
  bool _blur_request = false;
};

}
}
