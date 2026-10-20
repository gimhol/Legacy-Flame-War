#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::optional<std::vector<std::shared_ptr<IXMLElement>>> xml_x_hit_key_map(
    IXML& xml, const Value& map, const std::u16string& tag);
Value xml_2_hit_key_map(const IXMLElement& el, const std::u16string& tag);

}
}
}
