#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

Value xml_to_bg_terrain(const IXMLElement& el);

}
}
}
