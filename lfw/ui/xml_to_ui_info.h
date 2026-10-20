#pragma once

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw::ui {

Value xml_to_ui_info(const IXMLElement& el);

}
