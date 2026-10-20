#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_2_bg_layer(const IXMLElement& el, size_t index);
std::shared_ptr<IXMLElement> xml_x_bg_layer(IXML& xml, const Value& l,
                                            const std::u16string& tag);

}
}
}
