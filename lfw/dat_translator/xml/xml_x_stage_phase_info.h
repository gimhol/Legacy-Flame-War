#pragma once

#include <memory>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml.h"
#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {
namespace dat_translator {
namespace xml {

std::shared_ptr<IXMLElement> xml_x_sound_play_info(IXML& xml, const Value& s,
                                                   const std::u16string& tag);
Value xml_2_sound_play_info(const IXMLElement& el);
std::shared_ptr<IXMLElement> xml_x_stage_phase_info(IXML& xml, const Value& p,
                                                    const std::u16string& tag);
Value xml_2_stage_phase_info(const IXMLElement& el);

}
}
}
