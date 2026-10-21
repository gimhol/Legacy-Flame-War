#include "lfw/data/armor_info.h"

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

std::optional<bool> opt_bool(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const bool* const b = v != nullptr ? std::get_if<bool>(v) : nullptr;
  if (b == nullptr) return std::nullopt;
  return *b;
}

std::vector<std::u16string> texts(const Object& o, const char16_t* key) {
  std::vector<std::u16string> out;
  const Value* const v = o.get(key);
  const Array* const a = v != nullptr ? as_array(*v) : nullptr;
  if (a == nullptr) {
    if (v != nullptr && std::holds_alternative<std::u16string>(*v)) out.push_back(text(o, key));
    return out;
  }
  out.reserve(a->size());
  for (const Value& x : a->items()) out.push_back(to_string(x));
  return out;
}

}

ArmorInfo::ArmorInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = text(*o, u"name");
  _type = num(*o, u"type");
  _toughness = num(*o, u"toughness");
  _toughness_r_value = num(*o, u"toughness_r_value");
  _toughness_r_tick = num(*o, u"toughness_r_tick");
  _toughness_resting = num(*o, u"toughness_resting");
  _fireproof = num(*o, u"fireproof");
  _antifreeze = num(*o, u"antifreeze");
  _fulltime = opt_bool(*o, u"fulltime");
  _injury_ratio = num(*o, u"injury_ratio");
  _shaking_ratio = num(*o, u"shaking_ratio");
  _motionless_ratio = num(*o, u"motionless_ratio");
  _has_hit_sounds = o->get(u"hit_sounds") != nullptr;
  _has_dead_sounds = o->get(u"dead_sounds") != nullptr;
  _hit_sounds = texts(*o, u"hit_sounds");
  _dead_sounds = texts(*o, u"dead_sounds");
}

}