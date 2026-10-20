#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

Value preprocess_bg_data(Value& data, std::vector<std::u16string>* warnings = nullptr,
                         std::vector<std::u16string>* errors = nullptr);

}
}
