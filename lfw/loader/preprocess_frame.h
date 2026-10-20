#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

bool preprocess_frame(Value& ctx, std::u16string& error);

}
}
