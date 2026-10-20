#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/tool_xml_element.h"

namespace lfw {

class ToolXML : public IXML {
 public:
  std::shared_ptr<IXMLElement> parse(const std::u16string& text) override;
  std::shared_ptr<IXMLElement> create(const std::u16string& tag) override;
  std::shared_ptr<IXMLElement> from_string(const std::u16string& str,
                                           const std::u16string& tag) override;
  std::shared_ptr<IXMLElement> from_number(double num, const std::u16string& tag) override;
  std::shared_ptr<IXMLElement> from_boolean(bool v, const std::u16string& tag) override;
  std::shared_ptr<IXMLElement> from_array(const Value& arr, const std::u16string& tag) override;
  std::shared_ptr<IXMLElement> from_object(const Value& obj, const std::u16string& tag) override;

 private:
  std::shared_ptr<IXMLElement> from_value(const Value& value);
};

}
