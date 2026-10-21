#include "lfw/data/frame_pic.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace {

double num_or_nan(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::nan("");
}

std::optional<double> opt_num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const double n = v != nullptr ? to_number(*v) : std::nan("");
  if (std::isnan(n)) return std::nullopt;
  return n;
}

}

FramePic::FramePic(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  {
    const std::u16string* const s = std::get_if<std::u16string>(o->get(u"tex"));
    if (s != nullptr) _tex = *s;
  }
  _x = num_or_nan(*o, u"x");
  _y = num_or_nan(*o, u"y");
  _w = num_or_nan(*o, u"w");
  _h = num_or_nan(*o, u"h");
  _rad = opt_num(*o, u"rad");
  _deg = opt_num(*o, u"deg");
  _ox = opt_num(*o, u"ox");
  _oy = opt_num(*o, u"oy");
  _cx = opt_num(*o, u"cx");
  _cy = opt_num(*o, u"cy");
  _cos_r = field_or(*o, u"__cos_r");
  _sin_r = field_or(*o, u"__sin_r");
}

}