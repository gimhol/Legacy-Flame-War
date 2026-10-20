#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::shared_ptr<IXMLElement> xml_x_frame_indexes(IXML& xml, const Value& indexes,
                                                 const std::u16string& tag);
Value xml_2_frame_indexes(const IXMLElement* el);

}
}
}
