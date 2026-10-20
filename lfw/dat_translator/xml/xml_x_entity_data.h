#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::shared_ptr<IXMLElement> xml_x_entity_data(IXML& xml, const Value& data,
                                               const std::u16string& tag = u"entity");
Value xml_2_entity_data(const IXMLElement* el);

}
}
}
