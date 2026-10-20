#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

std::vector<Value> to_director_list(const Value& raw);
Value make_director_group(const Value& index, const Value& raw,
                          const std::u16string& file);

}
}
