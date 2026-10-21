#include "lfw/data/dialog_info.h"

#include <cmath>
#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

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

std::optional<bool> optional_bool(const Object& o, const char16_t* key) {
  const Value* const v = o.get(key);
  if (v == nullptr) return std::nullopt;
  if (const bool* const b = std::get_if<bool>(v)) return *b;
  if (const double* const n = std::get_if<double>(v)) return std::isfinite(*n) && *n != 0.0;
  return std::nullopt;
}

}

DialogInfo::DialogInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _type = text(*o, u"type");
  _fighter = optional_text(*o, u"fighter");
  _pause = optional_bool(*o, u"pause");
  _i18n = text(*o, u"i18n");
  _close_by = optional_text(*o, u"close_by");
  if (const Value* const v = o->get(u"hide_stats")) {
    const double n = to_number(*v);
    _hide_stats = std::isnan(n) ? std::numeric_limits<double>::quiet_NaN() : n;
  }
  if (const Array* const arr = as_array(field_or(*o, u"end_test"))) {
    _end_test.reserve(arr->size());
    for (const Value& v : arr->items()) {
      const std::u16string* const s = std::get_if<std::u16string>(&v);
      if (s != nullptr) _end_test.push_back(*s);
    }
  }
}

bool DialogInfo::same_as(const DialogInfo& other) const { return same_ref(_raw, other._raw); }

}
