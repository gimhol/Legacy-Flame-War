#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw::ui {

inline constexpr const char16_t* kValidateUIImgInfoTag = u"validate_ui_img_info";

bool validate_ui_img_info(const Value& any, std::vector<std::u16string>* errors = nullptr,
                          std::vector<std::u16string>* warnings = nullptr);

}
