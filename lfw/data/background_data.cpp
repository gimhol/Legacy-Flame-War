#include "lfw/data/background_data.h"

#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

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
}

bool BackgroundData::same_as(const BackgroundData& other) const {
  return same_ref(_raw, other._raw);
}

}
