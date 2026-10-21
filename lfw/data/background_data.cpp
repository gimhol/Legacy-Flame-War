#include "lfw/data/background_data.h"

#include <cmath>
#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

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

/** 与 TS 一致：`type` 非数字时落成负值，`y()` 里就没有任何 case 命中，返回 0。 */
ITerrainInfo to_terrain(const Value& v) {
  ITerrainInfo t;
  const Object* const o = as_object(v);
  if (o == nullptr) return t;
  const double type = num(*o, u"type");
  t.id = text(*o, u"id");
  t.name = text(*o, u"name");
  t.type = std::isnan(type) ? -1 : static_cast<int>(type);
  t.x1 = num(*o, u"x1");
  t.x2 = num(*o, u"x2");
  t.z1 = num(*o, u"z1");
  t.z2 = num(*o, u"z2");
  t.h1 = num(*o, u"h1");
  t.h2 = num(*o, u"h2");
  return t;
}

}

BackgroundData::BackgroundData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = to_string(field_or(*o, u"id"));
  _base = BgInfo(field_or(*o, u"base"));
  _dataset = KeyedMap(field_or(*o, u"dataset"));
  if (const Array* const list = as_array(field_or(*o, u"layers"))) {
    _layers.reserve(list->size());
    for (const Value& v : list->items()) _layers.push_back(BgLayerInfo(v));
  }
  if (const Array* const list = as_array(field_or(*o, u"terrain"))) {
    _terrain.reserve(list->size());
    for (const Value& v : list->items()) _terrain.push_back(to_terrain(v));
  }
}

bool BackgroundData::same_as(const BackgroundData& other) const {
  return same_ref(_raw, other._raw);
}

}
