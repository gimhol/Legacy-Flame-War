#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_2_opoint_multi(const IXMLElement* el);
std::shared_ptr<IXMLElement> xml_x_opoint_multi(IXML& xml, const Value& o,
                                                const std::u16string& tag);
Value xml_2_opoint(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_opoint(IXML& xml, const Value& o, const std::u16string& tag);

}
}
}
