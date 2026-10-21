#include "lfw/data/next_frame.h"

#include <cmath>

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

NextFrame::NextFrame(const Value& raw) : _raw(&raw) {
  const Object* const o = as_object(raw);
  _hp = num_field(o, u"hp");
  _mp = num_field(o, u"mp");
  _mp_mode = num_field(o, u"mp_mode");
}

bool NextFrame::valid() const { return _raw != nullptr && as_object(*_raw) != nullptr; }

bool NextFrame::is_str() const {
  return _raw != nullptr && std::holds_alternative<std::u16string>(*_raw);
}

bool NextFrame::is_list() const { return _raw != nullptr && as_array(*_raw) != nullptr; }

const std::u16string& NextFrame::str() const {
  static const std::u16string kEmpty;
  if (!is_str()) return kEmpty;
  return std::get<std::u16string>(*_raw);
}

const Value& NextFrame::id() const {
  const Object* const o = _raw != nullptr ? as_object(*_raw) : nullptr;
  const Value* const v = o != nullptr ? o->get(u"id") : nullptr;
  return v != nullptr ? *v : undefined_value();
}

const Value& NextFrame::to_value() const {
  return _raw != nullptr ? *_raw : undefined_value();
}

}