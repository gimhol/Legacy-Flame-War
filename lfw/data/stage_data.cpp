#include "lfw/data/stage_data.h"

#include <cmath>
#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

namespace {

std::optional<std::u16string> optional_text(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  const std::u16string* const s = std::get_if<std::u16string>(v);
  if (s != nullptr) return *s;
  return std::nullopt;
}

}

StageData::StageData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = to_string(field_or(*o, u"id"));
  _name = to_string(field_or(*o, u"name"));
  _title = optional_text(*o, u"title");
  _chapter = optional_text(*o, u"chapter");
  _next = optional_text(*o, u"next");
  _bg = optional_text(*o, u"bg");
  _cond_end = optional_text(*o, u"cond_end");
  _act_of_goto_next = optional_text(*o, u"act_of_goto_next");
  _starting_name = to_string(field_or(*o, u"starting_name"));
  if (const Value* const v = o->get(u"is_starting")) {
    if (const bool* const b = std::get_if<bool>(v)) {
      _is_starting = *b;
    } else if (const double* const n = std::get_if<double>(v)) {
      _is_starting = std::isfinite(*n) && *n != 0.0;
    }
  }
  if (const Array* const arr = as_array(field_or(*o, u"group"))) {
    _group.reserve(arr->size());
    for (const Value& v : arr->items()) {
      const std::u16string* const s = std::get_if<std::u16string>(&v);
      if (s != nullptr) _group.push_back(*s);
    }
  }
  if (const Array* const arr = as_array(field_or(*o, u"phases"))) {
    _phases.reserve(arr->size());
    for (const Value& v : arr->items()) _phases.push_back(StagePhaseInfo(v));
  }
}

bool StageData::same_as(const StageData& other) const { return same_ref(_raw, other._raw); }

}
