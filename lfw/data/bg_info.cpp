#include "lfw/data/bg_info.h"

#include <cmath>
#include <utility>

namespace lfw {
namespace {

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::nan("");
}

std::u16string text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

}

BgInfo::BgInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _name = text(*o, u"name");
  _shadow = text(*o, u"shadow");
  _shadow_w = num(*o, u"shadow_w");
  _shadow_h = num(*o, u"shadow_h");
  _left = num(*o, u"left");
  _right = num(*o, u"right");
  _far = num(*o, u"far");
  _near = num(*o, u"near");
  _height = num(*o, u"height");
  _zoom_x = num(*o, u"zoom_x");
  _zoom_y = num(*o, u"zoom_y");
  _zoom_z = num(*o, u"zoom_z");
}

bool BgLayerInfo::has(const char16_t* key) const {
  const Object* const o = as_object(_raw);
  return o != nullptr && o->get(key) != nullptr;
}

BgLayerInfo BgLayerInfo::with_x(double x) const {
  BgLayerInfo ret = *this;
  ret._x = x;
  return ret;
}

BgLayerInfo::BgLayerInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = text(*o, u"name");
  _file = text(*o, u"file");
  _absolute = num(*o, u"absolute");
  _opacity = num(*o, u"opacity");
  _width = num(*o, u"width");
  _height = num(*o, u"height");
  _x = num(*o, u"x");
  _y = num(*o, u"y");
  _z = num(*o, u"z");
  _w = num(*o, u"w");
  _h = num(*o, u"h");
  _dw = num(*o, u"dw");
  _dh = num(*o, u"dh");
  _uv_loop = num(*o, u"uv_loop");
  _loop = num(*o, u"loop");
  _cc = num(*o, u"cc");
  _c1 = num(*o, u"c1");
  _c2 = num(*o, u"c2");
  _offsetAnimX = num(*o, u"offsetAnimX");
  _offsetAnimY = num(*o, u"offsetAnimY");
  _color = o->get(u"color") != nullptr ? *o->get(u"color") : Value();
}

}
