#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

bool validate_ui_img_operation_crop(const Value& any, std::vector<std::u16string>* errors);

}
