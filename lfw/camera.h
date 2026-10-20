#pragma once

#include "lfw/core/value.h"
#include "lfw/defines/i_vector2.h"

namespace lfw {

class World;

class Camera {
 public:
  explicit Camera(World* world);

  World* world() const { return _world; }
  const Vector2* locked() const { return _has_locked ? &_locked : nullptr; }
  const Vector2* dested() const { return _has_dested ? &_dested : nullptr; }

  Vector2 destination;
  Vector2 position;
  Vector2 velocity;

  void reset();
  void jump_x(double x);
  void jump_y(double y);
  void undest();
  void dest(double x, double y);
  void unlock();
  void lock(double x, double y);
  void update();

 private:
  World* _world = nullptr;
  bool _has_locked = false;
  bool _has_dested = false;
  Vector2 _locked;
  Vector2 _dested;
};

}
