#include "lfw/data/director_data.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace director {

namespace {

std::u16string text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  const std::u16string* const s = v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
  return s != nullptr ? *s : std::u16string();
}

std::optional<std::u16string> optional_text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  const std::u16string* const s = std::get_if<std::u16string>(v);
  if (s != nullptr) return *s;
  return std::nullopt;
}

double num(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr || std::holds_alternative<std::monostate>(*v) ||
      std::holds_alternative<NullTag>(*v)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return to_number(*v);
}

std::vector<std::u16string> texts(const Object& o, const char16_t* key) {
  std::vector<std::u16string> out;
  const Value* const v = o.get(key);
  if (v == nullptr) return out;
  if (const std::u16string* const one = std::get_if<std::u16string>(v)) {
    out.push_back(*one);
    return out;
  }
  const Array* const arr = as_array(*v);
  if (arr == nullptr) return out;
  out.reserve(arr->size());
  for (const Value& item : arr->items()) {
    const std::u16string* const s = std::get_if<std::u16string>(&item);
    if (s != nullptr) out.push_back(*s);
  }
  return out;
}

bool is_platform_sep(char16_t c) {
  return c == u',' || c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}

void split_platforms(const std::u16string& raw, std::vector<std::u16string>& out) {
  std::u16string cur;
  for (const char16_t c : raw) {
    if (is_platform_sep(c)) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
}

}  // namespace

bool supports_platform(const Value& director, std::u16string_view platform) {
  const Value platforms = field_or(director, u"platform");
  if (std::holds_alternative<std::monostate>(platforms) ||
      std::holds_alternative<NullTag>(platforms)) {
    return true;
  }
  std::vector<std::u16string> tokens;
  if (const std::u16string* const one = std::get_if<std::u16string>(&platforms)) {
    split_platforms(*one, tokens);
  } else if (const Array* const arr = as_array(platforms)) {
    for (const Value& item : arr->items()) {
      if (const std::u16string* const s = std::get_if<std::u16string>(&item)) {
        split_platforms(*s, tokens);
      }
    }
  }
  if (tokens.empty()) return true;
  for (const std::u16string& token : tokens) {
    if (token == platform) return true;
  }
  return false;
}

ActorLayoutData::ActorLayoutData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _type = text(*o, u"type");
  _axis = text(*o, u"axis");
  _x = field_or(*o, u"x");
  _y = field_or(*o, u"y");
  _z = field_or(*o, u"z");
  _length = field_or(*o, u"length");
  _radius = field_or(*o, u"radius");
  _rx = field_or(*o, u"rx");
  _rz = field_or(*o, u"rz");
}

ActorData::ActorData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _oid = texts(*o, u"oid");
  _source = text(*o, u"source");
  _half = text(*o, u"half");
  _count = num(*o, u"count");
  _layout = ActorLayoutData(field_or(*o, u"layout"));
  _at = field_or(*o, u"at");
  _x = field_or(*o, u"x");
  _y = field_or(*o, u"y");
  _z = field_or(*o, u"z");
  _dx = field_or(*o, u"dx");
  _dy = field_or(*o, u"dy");
  _dz = field_or(*o, u"dz");
  _values = KeyedMap(field_or(*o, u"values"));
  _group = texts(*o, u"group");
  _ctrl = text(*o, u"ctrl");
  _player_id = text(*o, u"player_id");
  if (const Value* const v = o->get(u"attach")) {
    if (const bool* const b = std::get_if<bool>(v)) {
      _attach = *b;
    }
  }
}

DirectorData::DirectorData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _name = text(*o, u"name");
  _bg = optional_text(*o, u"bg");
  _stage = DirectorStageData(field_or(*o, u"stage"));
  _groups = KeyedMap(field_or(*o, u"groups"));
  _setup = field_or(*o, u"setup");
  _auto = field_or(*o, u"auto");
  _timeline = field_or(*o, u"timeline");
  if (const Array* const arr = as_array(field_or(*o, u"actors"))) {
    _actors.reserve(arr->size());
    for (const Value& v : arr->items()) _actors.push_back(ActorData(v));
  }
}

DirectorGroupData::DirectorGroupData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = text(*o, u"id");
  _name = optional_text(*o, u"name");
  _file = text(*o, u"file");
  if (const Array* const arr = as_array(field_or(*o, u"directors"))) {
    _directors.reserve(arr->size());
    for (const Value& v : arr->items()) _directors.push_back(DirectorData(v));
  }
}

}
}
