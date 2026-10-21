#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/ui_info.h"
#include "lfw/defines/i_vector3.h"
#include "lfw/ui/style.h"
#include "lfw/ui/ui_event.h"
#include "lfw/ui/ui_node_renderer.h"
#include "lfw/utils/is_class.h"
#include "lfw/utils/times.h"

namespace lfw {

class LFW;

namespace ui {

class UIComponent;
class UILayer;
class UINode;
struct UIPopPageOpts;

struct UINodeCallbacks {
  std::function<void(LFWPointerEvent&)> on_click;
  std::function<void(UINode&)> on_show;
  std::function<void(UINode&)> on_hide;
  std::function<void(UINode&)> on_foucs_changed;
  std::function<void(UINode*, UINode*)> on_foucs_item_changed;
  std::function<void(LFWPointerEvent&, UINode&)> on_pointer_down;
  std::function<void(LFWPointerEvent&, UINode&)> on_pointer_move;
  std::function<void(LFWPointerEvent&, UINode&)> on_pointer_up;
  std::function<void(LFWPointerEvent&, UINode&)> on_pointer_cancel;
  std::function<void(UINode&)> on_pointer_leave;
  std::function<void(UINode&)> on_pointer_enter;
  std::function<void(UIComponent&, UINode&)> on_component_add;
  std::function<void(UIComponent&, UINode&)> on_component_del;
};

enum class UIFind { No, Yes, Abort, End };

class UINode {
 public:
  static constexpr const char* TAG = "UINode";
  static const ClazzTag* class_tag();
  virtual const ClazzTag* clazz() const { return class_tag(); }

  UINode(LFW& lfw, const Value& data, UINode* parent = nullptr, UILayer* layer = nullptr);
  virtual ~UINode();

  LFW& lfw() { return *_lfw; }
  const Value& data() const { return _data; }
  /** `ICookedUIInfo` 的强类型视图：字段名/取值点集中在这份（附加，不影响 `data()`）。 */
  const CookedUIInfo& info() const { return _info; }
  const Value& raw() const { return _raw; }

  UINodeCallbacks callbacks;

  IUINodeRenderer* renderer = nullptr;

  Vector3 pos;
  Vector3 scale{1, 1, 1};
  Vector3 size;
  Vector3 center;

  std::u16string color;
  Style style;

  UINode& set_text_object(const Value& v);
  const Value& text() const { return _text; }
  UINode& set_text(const std::u16string& text);
  UINode& set_text(const std::u16string& text, const Value& style);
  const Value& image() const { return _image; }
  void set_image(Value v) { _image = std::move(v); }
  bool text_style_is_node_style() const { return _text_style_versioned; }

  double w() const { return size.x; }
  double h() const { return size.y; }
  UINode& set_w(double v);
  UINode& set_h(double v);
  UINode& resize(std::optional<double> x = {}, std::optional<double> y = {},
                 std::optional<double> z = {});
  UINode& set_x(double v);
  UINode& set_y(double v);
  UINode& set_z(double v);
  UINode& move_to(std::optional<double> x = {}, std::optional<double> y = {},
                  std::optional<double> z = {});
  UINode& set_cx(double v);
  UINode& set_cy(double v);
  UINode& set_cz(double v);
  UINode& set_center(std::optional<double> x = {}, std::optional<double> y = {},
                     std::optional<double> z = {});
  UINode& set_sx(double v);
  UINode& set_sy(double v);
  UINode& set_sz(double v);
  UINode& set_scale(std::optional<double> x = {}, std::optional<double> y = {},
                    std::optional<double> z = {});
  double x() const { return pos.x; }
  double y() const { return pos.y; }
  double z() const { return pos.z; }
  double cx() const { return center.x; }
  double cy() const { return center.y; }
  double cz() const { return center.z; }
  double sx() const { return scale.x; }
  double sy() const { return scale.y; }
  double sz() const { return scale.z; }

  const Vector3& global_pos() const;
  void set_global_pos(double x, double y, double z);
  UINode& move_to_global(double x, double y, double z);

  struct Cross {
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    double mid_x = 0;
    double mid_y = 0;
  };
  struct Rect {
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
  };
  struct Geo {
    double pos_x = 0;
    double pos_y = 0;
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
  };
  const Cross& cross() const;
  const Rect& rect() const;
  const Geo& geo() const;

  bool hit(double x, double y) const;

  bool focused() const { return root()._focused_node == this; }
  void set_focused(bool v);
  UINode* focused_node() const { return root()._focused_node; }
  void set_focused_node(UINode* val);
  void blur() { set_focused(false); }

  Value id() const;
  Value name() const;
  UINode& root() const { return *_root; }
  UILayer* layer() const { return _root->_layer; }
  double depth() const { return _parent != nullptr ? _parent->depth() + 1 : 0; }
  const Value& state() const { return _state; }

  bool self_visible() const { return _visible; }
  bool clip_children() const { return _clip_children; }
  void set_clip_children(bool v) { _clip_children = v; }
  bool visible() const;
  UINode& set_visible(bool v);
  std::u16string background() const;
  void set_background(std::optional<std::u16string> v) { _background = std::move(v); }
  std::u16string foreground() const;
  void set_foreground(std::optional<std::u16string> v) { _foreground = std::move(v); }
  double backgroundAlpha() const;
  void set_background_alpha(std::optional<double> v) { _backgroundAlpha = v; }
  double foregroundAlpha() const;
  void set_foreground_alpha(std::optional<double> v) { _foregroundAlpha = v; }
  const Value& outlineColor() const { return _outlineColor; }
  const Value& outlineWidth() const { return _outlineWidth; }
  const Value& outlineAlpha() const { return _outlineAlpha; }
  void set_outline_color(Value v) { _outlineColor = std::move(v); }
  void set_outline_width(Value v) { _outlineWidth = std::move(v); }
  void set_outline_alpha(Value v) { _outlineAlpha = std::move(v); }
  virtual bool disabled() const;
  bool self_disabled() const { return _disabled; }
  double global_opacity() const;
  double opacity() const;
  UINode& set_opacity(double v);

  UINode* parent() const { return _parent; }
  const std::vector<UINode*>& children() const { return _children; }
  UINode& add_child(UINode& node);
  UINode& add_children(const std::vector<UINode*>& nodes);

  UINode* find_child(const std::u16string& id);
  UINode* find_child_by_name(const std::u16string& name);
  UINode* search_node(const std::u16string& id);
  UINode* lookup_node(const std::u16string& id);
  UINode* find_parent_by_id(const std::u16string& id);
  UINode* find_parent(const std::function<bool(UINode&)>& match,
                      const std::function<void(UINode&, const std::vector<UINode*>&)>& handler = {});
  Value get_value(const std::u16string& name, bool lookup = true) const;

  const std::vector<UIComponent*>& components() const { return _components; }
  UINode& add_components(const std::vector<UIComponent*>& components);
  UINode& add_components(UIComponent& component) { return add_components({&component}); }
  UINode& del_components(const std::vector<UIComponent*>& components);
  UINode& del_components(UIComponent& component) { return del_components({&component}); }
  UIComponent* find_component(const ClazzTag* type,
                              const std::function<bool(UIComponent&)>& condition = {});
  UIComponent* find_component_by_id(const ClazzTag* type, const std::u16string& id);
  std::vector<UIComponent*> find_components(
      const ClazzTag* type, const std::function<UIFind(UIComponent&)>& condition = {});
  std::vector<UIComponent*> find_components_by_id(const ClazzTag* type, const std::u16string& id);
  UIComponent* search_component(const ClazzTag* type,
                                const std::function<bool(UIComponent&)>& condition = {});
  UIComponent* search_component_by_id(const ClazzTag* type, const std::u16string& id);
  std::vector<UIComponent*> search_components(
      const ClazzTag* type, const std::function<UIFind(UIComponent&)>& condition = {});
  UIComponent* lookup_component(const ClazzTag* type,
                                const std::function<bool(UIComponent&)>& condition = {});
  UIComponent* lookup_component_by_id(const ClazzTag* type, const std::u16string& id);
  bool traversal_components(const std::function<bool(UIComponent&, int)>& fn, int depth = 0);

  int pointer_over() const { return _pointer_over; }
  int pointer_down() const { return _pointer_down; }
  int click_flag() const { return _click_flag; }
  double lifetime() const { return _update_times.value(); }
  void on_pointer_down(LFWPointerEvent& e);
  void on_pointer_move(LFWPointerEvent& e);
  void on_pointer_up(LFWPointerEvent& e);
  void on_pointer_cancel(LFWPointerEvent& e);
  void on_pointer_leave();
  void on_pointer_enter();
  void on_show();
  void on_hide();
  void on_foucs();
  void on_blur();
  void invoke_all_on_show();
  void invoke_all_on_hide();
  void invoke_all_visible();

  void on_start();
  void on_stop();
  void on_resume();
  void on_pause();
  void on_click(LFWPointerEvent& e);
  void on_key_down(LFWKeyEvent& e);
  void on_key_up(LFWKeyEvent& e);
  bool pop_page(const UIPopPageOpts& opts);
  static std::unique_ptr<UINode> create(LFW& lfw, const Value& info, UINode* parent = nullptr,
                                        UILayer* layer = nullptr);

  virtual void update(double dt);

  UINode& set_disabled(bool v);

 private:
  void clear_caches();
  Value make_i18n_text(const Value& baked, const std::u16string& key);
  void auto_size_by_text(const Value& v);
  struct TextSizeKey {
    bool set = false;
    bool versioned = false;
    std::u16string text;
    std::u16string json;
    double version = 0;
  };
  TextSizeKey text_size_key(const Value& v) const;
  bool same_text_size_key(const TextSizeKey& a, const TextSizeKey& b) const;

  LFW* _lfw = nullptr;
  Value _data;
  CookedUIInfo _info;
  Value _raw;
  Value _text;
  Value _image;
  bool _text_style_versioned = false;
  TextSizeKey _auto_size_key;
  UINode* _root = nullptr;
  UILayer* _layer = nullptr;
  UINode* _focused_node = nullptr;
  UINode* _state_focused_node = nullptr;

  bool _visible = true;
  bool _disabled = false;
  bool _clip_children = false;
  Value _opacity = Value(1.0);
  Value _state = Value(std::make_shared<Object>());

  UINode* _parent = nullptr;
  std::vector<UINode*> _children;
  std::vector<std::unique_ptr<UINode>> _owned_children;
  std::vector<UIComponent*> _components;
  std::vector<std::unique_ptr<UIComponent>> _owned_components;
  bool _components_updating = false;
  std::vector<UIComponent*> _del_components;
  std::unique_ptr<IUINodeRenderer> _owned_renderer;
  Vector3 _prev_size;
  Vector3 _prev_center;
  Vector3 _prev_pos;

  mutable std::optional<Cross> _cache_cross;
  mutable std::optional<Rect> _cache_rect;
  mutable std::optional<Geo> _cache_geo;
  mutable std::optional<Vector3> _cache_global_pos;

  std::optional<std::u16string> _background;
  std::optional<double> _backgroundAlpha;
  std::optional<std::u16string> _foreground;
  std::optional<double> _foregroundAlpha;
  Value _outlineColor;
  Value _outlineWidth;
  Value _outlineAlpha;

  int _pointer_over = 0;
  int _pointer_down = 0;
  int _click_flag = 0;
  Times _update_times;
};

std::unordered_set<const UINode*>& live_uinodes();
inline bool is_live_uinode(const UINode* node) { return live_uinodes().count(node) != 0; }

}
}
