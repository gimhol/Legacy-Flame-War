#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ArrowSwitcher;

struct ArrowSwitcherCallbackArgs {
  double value = 0.0;
  ArrowSwitcher* self = nullptr;
};
using ArrowSwitcherCallbacks = CallbacksT<ArrowSwitcherCallbackArgs>;

class ArrowSwitcher : public UIComponent {
 public:
  static constexpr const char* TAG = "ArrowSwitcher";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  ArrowSwitcherCallbacks callbacks;

  void on_start() override;
  void on_resume() override;
  void on_stop() override;
  void update(double dt) override;

  std::vector<std::u16string> items();
  double count();
  double value();
  UINode* left();
  UINode* right();
  UINode* left_hl();
  UINode* right_hl();
  UINode* label();
  UINode* responser();
  void on_value_changed(std::function<void(double, ArrowSwitcher&)> cb);
  void press(double dir);
  void step(double dir);
  void set_value(double v);

 protected:
  void update_highlights(double dt);
  void sync_label();
  void sync_arrows();

  double _value = 0.0;
  double _hl_l = 0.0;
  double _hl_r = 0.0;
};

}
}
