#include "lfw/bot/is_ray_hit.h"

#include <variant>

#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"
#include "lfw/utils/type_cast.h"

namespace lfw {
namespace bot {
namespace {

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

double num_or(const Value& v, double fallback) {
  return is_undefined(v) ? fallback : to_number(v);
}

Value defaulted(const Value& v, const Value& fallback) {
  return is_undefined(v) ? fallback : v;
}

}

bool is_ray_hit(const Entity& a, const Entity& b, const BotRay& ray) {
  const double min_x = ray.min_x;
  const double max_x = ray.max_x;
  const double min_z = ray.min_z;
  const double max_z = ray.max_z;
  static const double default_max_d = defines::num(u"Defines.DAFUALT_QUBE_LENGTH_POW2");
  const double max_d = ray.has_max_d ? ray.max_d : default_max_d;

  const double facing = a.facing;
  const double dx = round_float(b.position.x - a.position.x);
  const double dz = round_float(b.position.z - a.position.z);

  if (!between(facing * dx, min_x, max_x)) return ray.reverse;
  if (!between(abs(dz), min_z, max_z)) return ray.reverse;

  const double rx = ray.x * facing;
  const double rz = ray.z;
  const double d_sq = round_float(rx * rx + rz * rz);
  if (d_sq == 0.0) return ray.reverse;

  const double cross = rx * dz - rz * dx;
  const bool hit = round_float((cross * cross) / d_sq) < max_d;
  return ray.reverse ? !hit : hit;
}

}
}
