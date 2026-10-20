#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::u16string xml_from_json(const Value& data, const std::u16string& tag_name,
                             const std::optional<std::vector<std::u16string>>& key_order =
                                 std::nullopt);

}
}
}
