#pragma once

namespace lfw {

struct Vector2 {
  double x = 0;
  double y = 0;

  Vector2() = default;
  Vector2(double px, double py) : x(px), y(py) {}

  void set(double px, double py) {
    x = px;
    y = py;
  }
};

}
