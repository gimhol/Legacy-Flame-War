#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_2_stage_info(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_stage_info(IXML& xml, const Value& s, const std::u16string& tag);
std::vector<Value> xml_to_stage_info_list(const IXMLElement& el);

}
}
}
