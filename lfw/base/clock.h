#pragma once

#include <functional>
#include <utility>

namespace lfw {

class IClock {
 public:
  virtual ~IClock() = default;
  virtual double now() const = 0;
  virtual int add(std::function<void()> handler) = 0;
  virtual void del(int handle) = 0;
  virtual bool hidden() const = 0;
};

class ITimeout {
 public:
  virtual ~ITimeout() = default;
  virtual int add(std::function<void()> handler, double timeout) = 0;
  virtual void del(int timer_id) = 0;
};

inline IClock*& clock_slot() {
  static IClock* slot = nullptr;
  return slot;
}

inline IClock* clock() { return clock_slot(); }

inline void set_clock(IClock* value) { clock_slot() = value; }

inline ITimeout*& timeout_slot() {
  static ITimeout* slot = nullptr;
  return slot;
}

inline ITimeout* timeout() { return timeout_slot(); }

inline void set_timeout(ITimeout* value) { timeout_slot() = value; }

inline double clock_now() { return clock() != nullptr ? clock()->now() : 0.0; }

inline bool clock_hidden() { return clock() != nullptr && clock()->hidden(); }

inline int clock_add(std::function<void()> handler) {
  return clock() != nullptr ? clock()->add(std::move(handler)) : 0;
}

inline void clock_del(int handle) {
  if (clock() != nullptr) clock()->del(handle);
}

inline int timeout_add(std::function<void()> handler, double delay) {
  return timeout() != nullptr ? timeout()->add(std::move(handler), delay) : 0;
}

inline void timeout_del(int timer_id) {
  if (timeout() != nullptr) timeout()->del(timer_id);
}

}
