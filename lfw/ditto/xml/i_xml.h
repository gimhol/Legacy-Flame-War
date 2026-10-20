#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {

class IXML {
 public:
  virtual ~IXML() = default;

  virtual std::shared_ptr<IXMLElement> parse(const std::u16string& text) = 0;
  virtual std::shared_ptr<IXMLElement> create(const std::u16string& tag) = 0;
  virtual std::shared_ptr<IXMLElement> from_string(const std::u16string& str,
                                                   const std::u16string& tag = u"string") = 0;
  virtual std::shared_ptr<IXMLElement> from_number(double num,
                                                   const std::u16string& tag = u"number") = 0;
  virtual std::shared_ptr<IXMLElement> from_boolean(bool v,
                                                    const std::u16string& tag = u"boolean") = 0;
  virtual std::shared_ptr<IXMLElement> from_array(const Value& arr,
                                                  const std::u16string& tag = u"array") = 0;
  virtual std::shared_ptr<IXMLElement> from_object(const Value& obj,
                                                   const std::u16string& tag = u"object") = 0;
};

}
