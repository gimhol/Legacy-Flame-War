#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::shared_ptr<IXMLElement> xml_x_dat_index(IXML& xml, const Value& idx,
                                             const std::u16string& tag = u"dat_index");
Value xml_2_dat_index(const IXMLElement& el);

}
}
}
