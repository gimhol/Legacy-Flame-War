#pragma once

#include <string>
#include <utility>

#include "lfw/defines/i_vector3.h"

namespace lfw::ui {


class IUIEvent {
 public:
  virtual ~IUIEvent() = default;
  virtual int stopped() const = 0;
  virtual void stop_propagation() = 0;
  virtual void stop_immediate_propagation() = 0;
};

class LFWUIEvent : public IUIEvent {
 public:
  int stopped() const override { return _stopped; }
  void stop_propagation() override { _stopped = 1; }
  void stop_immediate_propagation() override { _stopped = 2; }

 protected:
  int _stopped = 0;
};

class LFWPointerEvent : public LFWUIEvent {
 public:
  LFWPointerEvent(const Vector3& point, double button) : point(point), button(button) {}

  Vector3 point;
  double button = 0.0;
};

class LFWKeyEvent : public LFWUIEvent {
 public:
  LFWKeyEvent(std::u16string player, bool pressed, std::u16string game_key, std::u16string key)
      : player(std::move(player)),
        game_key(std::move(game_key)),
        key(std::move(key)),
        pressed(pressed) {}

  std::u16string player;
  std::u16string game_key;
  std::u16string key;
  bool pressed = false;
};

}
