#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ScrollView;
class SliderHandle;

struct ScrollViewCallbackArgs {
  double offset = 0.0;
  double factor = 0.0;
  ScrollView* self = nullptr;
};
using ScrollViewCallbacks = CallbacksT<ScrollViewCallbackArgs>;

class ScrollView : public UIComponent {
 public:
  static constexpr const char* TAG = "ScrollView";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  ScrollViewCallbacks callbacks;

  void on_start() override;
  void on_stop() override;
  void on_resume() override;
  void update(double dt) override;

  std::u16string direction();
  bool is_col();
  double step();
  double speed();
  bool draggable();
  bool keyboard();
  bool wheel_enabled();
  double wheel_speed();
  bool cull();
  double drag_threshold();
  bool follow_focus();

  UINode* content();
  void set_content(UINode* v);
  UINode* scrollbar_thumb();
  SliderHandle* scrollbar();

  double max_scroll();
  bool dragging();
  double offset();
  double target_offset() { return _target_offset; }
  void set_offset(double v);
  double factor();
  void set_factor(double v);

  ScrollView& scroll_to(double offset);
  ScrollView& scroll_by(double delta);
  ScrollView& scroll_to_start();
  ScrollView& scroll_to_end();
  void on_scroll(std::function<void(double, double, ScrollView&)> cb);
  bool ensure_visible(UINode* node);

 protected:
  double key_dir();
  void update_keys(double dt);
  UINode* focused_in(UINode* c);
  void unfocus_content();
  double pointer_axis_px(const LfwPointingEvent& e);
  bool pointer_in_view(const LfwPointingEvent& e);
  bool pointer_in_node(const LfwPointingEvent& e, UINode* n);
  void scrollbar_pointer(const LfwPointingEvent& e);
  void end_drag();
  void sync_scrollbar();
  void sync_thumb(UINode* thumb);

  std::pair<double, double> range();
  double window_lo();
  std::pair<double, double> box(UINode* c);
  std::pair<double, double> node_range(UINode* n);
  void apply_offset();
  void update_cull();
  bool is_culled(UINode* n) const;

  struct PointListener : public LfwPointingsCallback {
    explicit PointListener(ScrollView* o) : owner(o) {}
    ScrollView* owner = nullptr;
    void on_pointer_down(LfwPointingEvent& e) override;
    void on_pointer_move(LfwPointingEvent& e) override;
    void on_pointer_up(LfwPointingEvent& e) override;
    void on_pointer_cancel(LfwPointingEvent& e) override;
    void on_wheel(LfwPointingEvent& e) override;
  };

  UINode* _content = nullptr;
  bool _captured = false;
  double _base_x = 0;
  double _base_y = 0;

  double _offset = 0;
  double _target_offset = 0;
  double _reported = -1;

  std::vector<UINode*> _culled;
  std::map<UINode*, bool> _cull_orig;

  bool _armed = false;
  bool _dragging = false;
  double _last_axis = 0;
  double _drag_dist = 0;

  bool _key_active = false;
  double _key_time = 0;

  std::optional<double> _last_slider_factor;
  SliderHandle* _scrollbar = nullptr;
  UINode* _sb_thumb = nullptr;
  bool _sb_dragging = false;
  double _sb_grab = 0;

  PointListener _p{this};
};

}
}
