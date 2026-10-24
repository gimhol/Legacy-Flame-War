#pragma once

#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>

#include "lfw/core/js_num.h"

namespace lfw {

inline double floor(double x) { return js_floor(x); }
inline double ceil(double x) { return js_ceil(x); }
inline double abs(double x) { return js_abs(x); }
inline double round(double x) { return js_round(x); }

inline bool between(double v, double lo, double hi) { return v >= lo && v <= hi; }

inline double max(std::initializer_list<double> xs) {
  double r = -std::numeric_limits<double>::infinity();
  for (double x : xs) {
    if (std::isnan(x)) return x;
    if (x > r || (x == 0.0 && r == 0.0 && !std::signbit(x))) r = x;
  }
  return r;
}

inline double min(std::initializer_list<double> xs) {
  double r = std::numeric_limits<double>::infinity();
  for (double x : xs) {
    if (std::isnan(x)) return x;
    if (x < r || (x == 0.0 && r == 0.0 && std::signbit(x))) r = x;
  }
  return r;
}

// 两参数版本单独写：`max({a,b})` 每次都要构造 initializer_list 再循环，而这两个函数
// 在物理/回血/受伤里每帧被调几十万次。语义与上面的列表版本逐位一致（含 NaN 与 ±0）。
inline double max(double a, double b) {
  if (std::isnan(a)) return a;
  if (std::isnan(b)) return b;
  double r = a;
  if (b > r || (b == 0.0 && r == 0.0 && !std::signbit(b))) r = b;
  return r;
}

inline double min(double a, double b) {
  if (std::isnan(a)) return a;
  if (std::isnan(b)) return b;
  double r = a;
  if (b < r || (b == 0.0 && r == 0.0 && std::signbit(b))) r = b;
  return r;
}

inline double sqrt(double x) { return std::sqrt(x); }
inline double cos(double x) { return std::cos(x); }
inline double sin(double x) { return std::sin(x); }
inline double tan(double x) { return std::tan(x); }
inline double acos(double x) { return std::acos(x); }
inline double pow(double x, double y) { return std::pow(x, y); }

inline double sign(double x) {
  if (std::isnan(x)) return x;
  if (x == 0.0) return x;
  return x < 0.0 ? -1.0 : 1.0;
}

inline constexpr double PI = std::numbers::pi;

}
