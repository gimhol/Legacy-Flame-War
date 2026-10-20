#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::u16string xml_from_stage_info(IXML& xml, const Value& stages);

}
}
}
