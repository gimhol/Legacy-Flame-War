#pragma once

#include <functional>
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

using XmlElementParser = std::function<Value(const IXMLElement&)>;
using XmlElementParserIdx = std::function<Value(const IXMLElement&, size_t)>;
using XmlElementCreator =
    std::function<std::shared_ptr<IXMLElement>(IXML&, const Value&, const std::u16string&)>;

Value xml_2_arr(const IXMLElement& el, const std::u16string& tag,
                const XmlElementParser& parser);
Value xml_2_arr(const IXMLElement& el, const std::u16string& tag,
                const XmlElementParserIdx& parser);
Value xml_2_non_empty(const IXMLElement& el, const std::u16string& tag,
                      const XmlElementParser& parser);
Value xml_2_non_empty(const IXMLElement& el, const std::u16string& tag,
                      const XmlElementParserIdx& parser);

std::optional<std::vector<std::shared_ptr<IXMLElement>>> xml_x_non_empty(
    IXML& xml, const Value& arr, const std::u16string& tag, const XmlElementCreator& creator,
    const std::shared_ptr<IXMLElement>& parent = nullptr);

}
}
}
