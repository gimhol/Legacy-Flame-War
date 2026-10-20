#pragma once

#include <vector>

#include "lfw/core/value.h"

namespace lfw {

bool read_nums(const Value& src, double len, const Value& fallbacks, std::vector<Value>& out,
               std::u16string* error = nullptr);

}
