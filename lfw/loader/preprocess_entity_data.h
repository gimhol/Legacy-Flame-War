#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

bool preprocess_entity_data(Value& ctx, std::u16string& error);

}
}
