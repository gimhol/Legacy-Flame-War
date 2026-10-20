#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"
#include "lfw/dat_translator/xml/xml_x_non_empty.h"

namespace lfw {
namespace dat_translator {
namespace xml {

using XmlMapReader = std::function<Value(const IXMLElement&)>;

Value xml_2_map(const IXMLElement& el, const std::vector<std::u16string>& tags,
                const XmlMapReader& reader);

std::optional<std::vector<std::shared_ptr<IXMLElement>>> xml_x_map(
    IXML& xml, const Value& map, const std::u16string& tag, const XmlElementCreator& writer,
    const std::shared_ptr<IXMLElement>& parent = nullptr);

}
}
}
