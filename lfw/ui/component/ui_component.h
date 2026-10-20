#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lfw/base/debugging.h"
#include "lfw/core/value.h"
#include "lfw/defines/i_vector3.h"
#include "lfw/ui/component/ui_props.h"
#include "lfw/ui/ui_event.h"
#include "lfw/utils/is_class.h"

namespace lfw {

class FSM;
class Keys;
class LFW;
class World;

namespace ui {

class UINode;


class UIComponent : public IDebugging {
 public:
  static constexpr const char* TAG = "UIComponent";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();
  static const Value& PROPS();

  UIComponent(UINode& layout, const std::u16string& f_name, const Value& info);
  virtual ~UIComponent() = default;

  virtual const ClazzTag* clazz() const { return class_tag(); }

  UINode& node;
  std::u16string f_name;
  Value info;
  UIProps props_holder;

  bool stopped = true;
  bool paused = true;
  bool mounted = false;
  std::u16string id;
  std::u16string name;

  bool ensure_props();
  const Value* props();
  bool props_failed() const { return _props_error; }
  const std::vector<std::u16string>& props_errors() const { return props_holder.errors(); }
  virtual const std::u16string& props_tag() const;
  virtual const Value& props_meta() const;

  bool enabled() const { return _enabled; }
  void set_enabled(bool v) { _enabled = v; }
  bool disabled() const { return !_enabled; }
  double LR() const;
  double UD() const;
  std::u16string node_name() const;
  LFW& lfw();
  World& world();
  Keys& keys() const;
  void recycle_keys();

  std::optional<double> num(size_t idx);
  std::optional<std::u16string> str(size_t idx);
  bool bool_(size_t idx);
  std::optional<std::vector<double>> nums(size_t idx, size_t length);
  std::optional<Vector3> vec3(size_t idx);

  UINode* find_node(const Value& which);

  virtual void init() {}  virtual void on_add() {}
  virtual void on_del() {}
  virtual void on_start() {}
  virtual void on_stop() {}
  virtual void on_resume() {}
  virtual void on_pause() {}
  virtual void on_show() {}
  virtual void on_hide() {}
  virtual void on_foucs() {}
  virtual void on_blur() {}
  virtual void update(double dt) { (void)dt; }
  virtual void on_click(LFWPointerEvent& e) { (void)e; }
  virtual void on_pointer_down(LFWPointerEvent& e) { (void)e; }
  virtual void on_pointer_move(LFWPointerEvent& e) { (void)e; }
  virtual void on_pointer_up(LFWPointerEvent& e) { (void)e; }
  virtual void on_pointer_cancel(LFWPointerEvent& e) { (void)e; }
  virtual void on_pointer_leave() {}
  virtual void on_pointer_enter() {}
  virtual void on_key_down(LFWKeyEvent& e) { (void)e; }
  virtual void on_key_up(LFWKeyEvent& e) { (void)e; }

  virtual bool is_playable() const { return false; }
  virtual void start() {}
  virtual void stop() {}
  virtual void replay() {}

  virtual FSM* fsm() { return nullptr; }

  bool __debugging = false;
  void debug(const std::u16string& func, const std::vector<Value>& args = {}) override;
  void warn(const std::u16string& func, const std::vector<Value>& args = {}) override;
  void log(const std::u16string& func, const std::vector<Value>& args = {}) override;

  bool debugging_enabled() const override { return __debugging; }
  const std::u16string& debugging_id() const override { return id; }
  const std::u16string& debugging_tag() const override { return props_tag(); }
  void debugging_out(DebugLevel level, const std::vector<Value>& args) override;

 private:
  struct NumsCache {
    bool set = false;
    bool failed = false;
    std::vector<double> nums;
  };
  struct Vec3Cache {
    bool set = false;
    bool failed = false;
    Vector3 v;
  };

  bool _enabled = true;
  bool _props_error = false;
  Value _props;
  mutable Keys* _keys = nullptr;
  std::map<std::u16string, NumsCache> _nums_caches;
  std::map<std::u16string, Vec3Cache> _vec3_caches;
};

}
}
