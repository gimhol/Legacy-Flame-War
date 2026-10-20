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

std::shared_ptr<IXMLElement> xml_x_director_index(IXML& xml, const Value& idx,
                                                  const std::u16string& tag = u"directors");
Value xml_2_director_index(const IXMLElement& el);

Value xml_2_director(const IXMLElement& el);
std::vector<Value> xml_to_directors(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_director(IXML& xml, const Value& data,
                                            const std::u16string& tag = u"director");
std::shared_ptr<IXMLElement> xml_x_directors(IXML& xml, const Value& group);

}
}
}
