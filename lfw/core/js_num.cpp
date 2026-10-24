#include "js_num.h"

#include <bit>
#include <cmath>

namespace lfw {

uint32_t js_to_uint32(double x) {
  if (!std::isfinite(x) || x == 0.0) return 0;
  const double t = std::trunc(x);
  double m = std::fmod(t, 4294967296.0);
  if (m < 0.0) m += 4294967296.0;
  return static_cast<uint32_t>(m);
}

int32_t js_to_int32(double x) {
  const uint32_t u = js_to_uint32(x);
  return u < 0x80000000u
             ? static_cast<int32_t>(u)
             : static_cast<int32_t>(static_cast<int64_t>(u) - 4294967296ll);
}

uint64_t f64_bits(double x) {
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
  return std::bit_cast<uint64_t>(x);
}

double f64_from_bits(uint64_t bits) { return std::bit_cast<double>(bits); }

}
