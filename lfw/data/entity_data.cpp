#include "lfw/data/entity_data.h"

#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

namespace {

const Object& empty_object() {
  static const Object o;
  return o;
}

double num_of(const Value& v) { return truthy(v) ? to_number(v) : 0.0; }

}

EntityData::EntityData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = to_string(field_or(*o, u"id"));
  _alias_id = to_string(field_or(*o, u"alias_id"));
  _type = num_of(field_or(*o, u"type"));
  _processed = truthy(field_or(*o, u"processed"));
  _on_dead = field_or(*o, u"on_dead");
  _on_exhaustion = field_or(*o, u"on_exhaustion");
  _pre_hitkeys = field_or(*o, u"pre_hitkeys");
  _post_hitkeys = field_or(*o, u"post_hitkeys");
  _pre_hitkeys_map = field_or(*o, u"__pre_hitkeys_map");
  _post_hitkeys_map = field_or(*o, u"__post_hitkeys_map");
  _bdy_prefabs = field_or(*o, u"bdy_prefabs");
  _itr_prefabs = field_or(*o, u"itr_prefabs");
}

double EntityData::base_type() const {
  const Value t = field_or(base(), u"type");
  return truthy(t) ? to_number(t) : 0.0;
}

const Object& EntityData::base() const {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return empty_object();
  const Object* const base_o = as_object(field_or(*o, u"base"));
  return base_o != nullptr ? *base_o : empty_object();
}

const Object& EntityData::frames() const {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return empty_object();
  const Object* const frames_o = as_object(field_or(*o, u"frames"));
  return frames_o != nullptr ? *frames_o : empty_object();
}

const Object& EntityData::indexes() const {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return empty_object();
  const Object* const idx = as_object(field_or(*o, u"indexes"));
  return idx != nullptr ? *idx : empty_object();
}

bool EntityData::same_as(const EntityData& other) const { return same_ref(_raw, other._raw); }

bool EntityData::has_field(const char16_t* key) const {
  const Object* const o = as_object(_raw);
  return o != nullptr && o->get(std::u16string(key)) != nullptr;
}

}
