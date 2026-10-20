#pragma once

#include <functional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value merge_by_tag(const IXMLElement& el, const std::u16string& tag,
                   const std::function<Value(const IXMLElement&)>& parser);
Value merge_by_tag(const IXMLElement& el, const std::u16string& tag,
                   const std::function<Value(const IXMLElement&)>& parser, Value target);

}
}
}
