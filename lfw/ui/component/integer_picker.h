#pragma once

#include <set>
#include <string>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/core/js_num.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class IntegerPicker;

struct IntegerPickerCallbackArgs {
  double curr = 0.0;
  double prev = 0.0;
  IntegerPicker* self = nullptr;
};
using IntegerPickerCallbacks = CallbacksT<IntegerPickerCallbackArgs>;

class IntegerPicker : public UIComponent {
 public:
  static constexpr const char* TAG = "IntegerPicker";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  IntegerPickerCallbacks callbacks;

  void init() override;
  void on_click(LFWPointerEvent& e) override;
  void on_key_down(LFWKeyEvent& e) override;

  double val() const { return _val; }
  double min() const { return _min; }
  double max() const { return _max; }

  void goto_next();
  void goto_prev();
  void set_min(double v);
  void set_max(double v);
  void set_val(double v);

 protected:
  double _min = kMinSafeInteger;
  double _max = kMaxSafeInteger;
  double _val = _min;
  std::set<std::u16string> _triggers;
};

}
}
