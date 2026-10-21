#include "lfw/data/next_frame.h"

#include <cmath>
#include <utility>

namespace lfw {
namespace {

const Value& undefined_value() {
  static const Value v;
  return v;
}

double num_field(const Object* const o, const char16_t* key) {
  const Value* const v = o != nullptr ? o->get(key) : nullptr;
  return v != nullptr ? to_number(*v) : std::nan("");
}

}

NextFrame::NextFrame(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  const Value* const id_v = o != nullptr ? o->get(u"id") : nullptr;
  if (const Array* const a = id_v != nullptr ? as_array(*id_v) : nullptr) {
    for (const Value& v : a->items()) _ids.push_back(to_string(v));
  } else if (const std::u16string* const s = id_v != nullptr ? std::get_if<std::u16string>(id_v) : nullptr) {
    _ids.push_back(*s);
  }
  _hp = num_field(o, u"hp");
  _mp = num_field(o, u"mp");
  _mp_mode = num_field(o, u"mp_mode");
}

bool NextFrame::valid() const { return as_object(_raw) != nullptr; }

bool NextFrame::is_str() const {
  return std::holds_alternative<std::u16string>(_raw);
}

bool NextFrame::is_list() const { return as_array(_raw) != nullptr; }

const std::u16string& NextFrame::str() const {
  static const std::u16string kEmpty;
  if (!is_str()) return kEmpty;
  return std::get<std::u16string>(_raw);
}

bool NextFrame::has_id() const { return !_ids.empty(); }

const Value& NextFrame::id_value() const {
  const Object* const o = as_object(_raw);
  const Value* const v = o != nullptr ? o->get(u"id") : nullptr;
  return v != nullptr ? *v : undefined_value();
}

const Value& NextFrame::to_value() const { return _raw; }

}