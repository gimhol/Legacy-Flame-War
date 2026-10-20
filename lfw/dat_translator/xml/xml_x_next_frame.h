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

std::shared_ptr<IXMLElement> xml_x_next_frame(IXML& xml, const Value& i, const std::u16string& tag);
Value xml_2_next_frame(const IXMLElement& el);

std::vector<std::shared_ptr<IXMLElement>> xml_x_t_next_frame(
    IXML& xml, const Value& nf, const std::u16string& tag,
    const std::shared_ptr<IXMLElement>& parent = nullptr);
Value xml_2_t_next_frame(const std::vector<IXMLElement*>& els);

}
}
}
