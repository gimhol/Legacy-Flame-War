#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

void xml_x_difficulty_map(const std::shared_ptr<IXMLElement>& el, const std::u16string& attr,
                          const Value& map);
Value xml_2_difficulty_map(const IXMLElement& el, const std::u16string& attr);

}
}
}
