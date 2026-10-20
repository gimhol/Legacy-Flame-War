#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_2_partial_world_dataset(const IXMLElement* el);
std::shared_ptr<IXMLElement> xml_x_partial_world_dataset(IXML& xml, const Value& i,
                                                         const std::u16string& tag);

}
}
}
