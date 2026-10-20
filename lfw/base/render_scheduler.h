#pragma once

#include <functional>

namespace lfw {

class IRenderScheduler {
 public:
  virtual ~IRenderScheduler() = default;
  virtual int add(std::function<void()> handler) = 0;
  virtual void del(int handle) = 0;
};

inline IRenderScheduler*& render_scheduler_slot() {
  static IRenderScheduler* slot = nullptr;
  return slot;
}

inline IRenderScheduler* render_scheduler() { return render_scheduler_slot(); }

inline void set_render_scheduler(IRenderScheduler* value) { render_scheduler_slot() = value; }

inline int render_add(std::function<void()> handler) {
  return render_scheduler() != nullptr ? render_scheduler()->add(std::move(handler)) : 0;
}

inline void render_del(int handle) {
  if (render_scheduler() != nullptr) render_scheduler()->del(handle);
}

}
