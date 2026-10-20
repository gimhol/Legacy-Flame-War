#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::shared_ptr<IXMLElement> xml_x_entity_info(IXML& xml, const Value& info,
                                               const std::u16string& tag);
Value xml_2_entity_info(const IXMLElement& el);

}
}
}
