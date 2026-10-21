#include "lfw/data/background_data.h"

#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

namespace {

const Object& empty_object() {
  static const Object o;
  return o;
}

}

BackgroundData::BackgroundData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = to_string(field_or(*o, u"id"));
  _base = field_or(*o, u"base");
  _dataset = field_or(*o, u"dataset");
  _layers = field_or(*o, u"layers");
}

const Object& BackgroundData::base() const {
  const Object* const o = as_object(_base);
  return o != nullptr ? *o : empty_object();
}

const Object& BackgroundData::dataset() const {
  const Object* const o = as_object(_dataset);
  return o != nullptr ? *o : empty_object();
}

bool BackgroundData::same_as(const BackgroundData& other) const {
  return same_ref(_raw, other._raw);
}

}
