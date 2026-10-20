#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class SliderHandle;

struct SliderHandleCallbackArgs {
  double value = 0.0;
  SliderHandle* self = nullptr;
};
using SliderHandleCallbacks = CallbacksT<SliderHandleCallbackArgs>;

class SliderHandle : public UIComponent {
 public:
  static constexpr const char* TAG = "SliderHandle";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  SliderHandleCallbacks callbacks;

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;

  std::u16string direction();
  virtual std::u16string mode();
  void set_mode(const std::u16string& v);
  std::vector<std::u16string> items();
  void set_items(const std::vector<std::u16string>& v);
  UINode* container();
  UINode* responser();
  UINode* handle_label();
  UINode* items_container();
  double step();
  void set_step(double v);
  double precision();
  void set_precision(double v);
  double min_value();
  void set_min_value(double v);
  double max_value();
  void set_max_value(double v);
  double factor();
  void set_factor(double v);
  double value();
  void set_value(double v);
  void on_value_changed(std::function<void(double, SliderHandle&)> cb);
  void handle_pointing_event(const LfwPointingEvent& e);

 protected:
  void on_p_down(LFWPointerEvent& e);

  struct PointListener : public LfwPointingsCallback {
    explicit PointListener(SliderHandle* o) : owner(o) {}
    SliderHandle* owner = nullptr;
    void on_pointer_down(LfwPointingEvent& e) override;
    void on_pointer_move(LfwPointingEvent& e) override;
    void on_pointer_up(LfwPointingEvent& e) override;
    void on_pointer_cancel(LfwPointingEvent& e) override;
  };

  bool _on_me = false;
  double _factor = 0.0;
  PointListener _b{this};
};

}
}
