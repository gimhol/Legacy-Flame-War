#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_2_drink_info(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_drink_info(IXML& xml, const Value& d,
                                              const std::u16string& tag);

}
}
}
