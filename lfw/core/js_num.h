#pragma once

#include <cmath>
#include <cstdint>

namespace lfw {

inline constexpr double kMaxSafeInteger = 9007199254740991.0;
inline constexpr double kMinSafeInteger = -9007199254740991.0;

/**
 * JS 语义的 `Math.round` / `Math.floor` / `Math.ceil` / `Math.abs`。
 *
 * 这几个在**每次距离计算**里都会被调到（`helper::manhattan_xz` → `abs` + `round_float`），
 * 目标搜索一帧几十万次；原来放在 .cpp 里是跨编译单元调用（无 LTO ⇒ 每个都不内联），
 * 调用开销比运算本身还大。语义一字未改，只是挪进头文件让编译器内联。
 */
inline double js_round(double x) {
  if (std::isnan(x) || std::isinf(x) || x == 0.0) return x;
  if (x > 0.0 && x < 0.5) return 0.0;
  if (x < 0.0 && x >= -0.5) return -0.0;
  double r = std::floor(x + 0.5);
  if (r - x > 0.5) r -= 1.0;
  return r;
}
inline double js_floor(double x) { return std::floor(x); }
inline double js_ceil(double x) { return std::ceil(x); }
inline double js_abs(double x) { return std::fabs(x); }

uint32_t js_to_uint32(double x);
int32_t js_to_int32(double x);

uint64_t f64_bits(double x);
double f64_from_bits(uint64_t bits);

}
