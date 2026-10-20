#pragma once

#include <cstddef>

#include "lfw/core/value.h"

namespace lfw::ui {


void spread_into(Object& dst, const Value& src);

Value copy_object(const Value& src);

const Value* field_of(const Value& v, const char16_t* key);

size_t js_key_count(const Value& v);

}
