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

Value xml_2_model_info(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_model_info(IXML& xml, const Value& f,
                                              const std::u16string& tag);
Value xml_2_model_info_map(const IXMLElement& el, const std::u16string& tag);
std::optional<std::vector<std::shared_ptr<IXMLElement>>> xml_x_model_info_map(
    IXML& xml, const Value& map, const std::u16string& tag);

}
}
}
