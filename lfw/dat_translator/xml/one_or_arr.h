#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value one_or_arr(const Value& mess);
Value non_empty(const Value& mess);

Value one_or_arr(const std::optional<std::vector<std::u16string>>& mess);
Value non_empty(const std::optional<std::vector<std::u16string>>& mess);

}
}
}
