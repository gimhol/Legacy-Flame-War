#include "lfw/data/picture_info.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {

namespace {

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  return v != nullptr ? to_number(*v) : std::numeric_limits<double>::quiet_NaN();
}

std::u16string text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

std::vector<std::u16string> texts(const Object& o, const char16_t* key) {
  std::vector<std::u16string> out;
  const Value* const v = o.get(key);
  const Array* const arr = v != nullptr ? as_array(*v) : nullptr;
  if (arr == nullptr) return out;
  out.reserve(arr->size());
  for (const Value& item : arr->items()) {
    const std::u16string* const s = std::get_if<std::u16string>(&item);
    if (s != nullptr) out.push_back(*s);
  }
  return out;
}

}

PictureInfo::PictureInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _path = text(*o, u"path");
  _variants = texts(*o, u"variants");
  _row = num(*o, u"row");
  _col = num(*o, u"col");
  _cell_w = num(*o, u"cell_w");
  _cell_h = num(*o, u"cell_h");
}

ModelInfo::ModelInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _path = text(*o, u"path");
  _variants = texts(*o, u"variants");
  const auto sub = [o](const char16_t* key) {
    const Value* const v = o->get(key);
    return Vec3Scale(v != nullptr ? *v : Value());
  };
  _rotation = sub(u"rotation");
  _scale = sub(u"scale");
  _offset = sub(u"offset");
}

PictureInfoMap read_picture_info_map(const Value& map) {
  PictureInfoMap out;
  const Object* const o = as_object(map);
  if (o == nullptr) return out;
  for (const std::u16string& key : o->keys()) {
    const Value* const v = o->get(key);
    if (v == nullptr) continue;
    out.emplace(key, PictureInfo(*v));
  }
  return out;
}

ModelInfoMap read_model_info_map(const Value& map) {
  ModelInfoMap out;
  const Object* const o = as_object(map);
  if (o == nullptr) return out;
  for (const std::u16string& key : o->keys()) {
    const Value* const v = o->get(key);
    if (v == nullptr) continue;
    out.emplace(key, ModelInfo(*v));
  }
  return out;
}

}
