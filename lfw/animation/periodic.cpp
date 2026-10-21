#include "lfw/animation/periodic.h"

#include <cmath>

#include "lfw/core/js_num.h"

namespace lfw {

namespace {

bool is_num(double v) { return !std::isnan(v) && std::isfinite(v); }

}

Periodic::Periodic(double bottom, double height, double scale) {
  set_duration(kMaxSafeInteger);
  if (is_num(bottom)) _b = bottom;
  if (is_num(height)) _h = height;
  if (is_num(scale)) _s = scale;
}

Periodic& Periodic::set_offset(double v) {
  offset = v;
  return *this;
}

Periodic& Periodic::set_scale(double v) {
  if (is_num(v)) _s = v;
  return *this;
}

Periodic& Periodic::set(double bottom, double height, double scale) {
  if (is_num(bottom)) _b = bottom;
  if (is_num(height)) _h = height;
  if (is_num(scale)) _s = scale;
  calc();
  return *this;
}

void Periodic::set_bottom(double v) {
  if (is_num(v)) _b = v;
}

void Periodic::set_height(double v) {
  if (is_num(v)) _h = v;
}

Animation& Periodic::calc() {
  set_value(method(offset + time() * scale()));
  return *this;
}

}
