#include "lfw/data/vec3_scale.h"

#include <utility>

namespace lfw {

namespace {

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::numeric_limits<double>::quiet_NaN();
}

}

Vec3Scale::Vec3Scale(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
}

}
