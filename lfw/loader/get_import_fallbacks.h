#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

bool get_import_fallbacks(const Value& name, std::vector<std::u16string>& fallbacks,
                          std::u16string& suffix);

}
}
