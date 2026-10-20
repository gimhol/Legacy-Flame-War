#pragma once

namespace lfw {
namespace ui {

class UINode;

struct UINodeRenderCalls {
  long long calls = 0;
  long long unhandled = 0;
};

inline UINodeRenderCalls& ui_node_render_calls() {
  static UINodeRenderCalls calls;
  return calls;
}

class IUINodeRenderer {
 public:
  virtual ~IUINodeRenderer() = default;

  UINode* ui = nullptr;

  virtual void del_self() {}
  virtual void on_start() {}
  virtual void on_stop() {}
  virtual void on_resume() {}
  virtual void on_pause() {}
  virtual void on_show() {}
  virtual void on_hide() {}
  virtual void on_foucs() {}
  virtual void on_blur() {}

  virtual void render(double dt, double dfactor) {
    static_cast<void>(dt);
    static_cast<void>(dfactor);
    ++ui_node_render_calls().unhandled;
  }
  virtual void add(IUINodeRenderer& child) { static_cast<void>(child); }
  virtual void del(IUINodeRenderer& child) { static_cast<void>(child); }
  virtual IUINodeRenderer* parent() { return nullptr; }
  virtual void set_x(double v) { static_cast<void>(v); }
  virtual void set_y(double v) { static_cast<void>(v); }
  virtual bool visible() const { return true; }
  virtual void set_visible(bool v) { static_cast<void>(v); }
};

}
}
