#pragma once

namespace lfw {

struct Vector3 {
  double x = 0;
  double y = 0;
  double z = 0;

  Vector3() = default;
  Vector3(double px, double py, double pz) : x(px), y(py), z(pz) {}

  void set(double px, double py, double pz) {
    x = px;
    y = py;
    z = pz;
  }

  bool equals(const Vector3& o) const {
    return x == o.x && y == o.y && z == o.z;
  }

  Vector3 clone() const { return *this; }

  Vector3 sub(const Vector3& o) const { return Vector3(x - o.x, y - o.y, z - o.z); }
};

}
