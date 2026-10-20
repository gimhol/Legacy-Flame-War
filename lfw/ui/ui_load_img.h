#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

class LFW;

namespace ui {

bool ui_load_img(LFW& lfw, const Value& img, Value& out, std::u16string& error,
                 bool* host_error = nullptr);

}
}
