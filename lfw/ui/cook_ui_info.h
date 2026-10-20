#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

class LFW;

namespace ui {

void find_ui_template(LFW& lfw, const Value* parent, const std::u16string& template_name,
                      Value& out);

Value merge_ui_template(LFW& lfw, const Value& raw_info, const Value* parent);

bool cook_ui_info(LFW& lfw, const Value& info, const Value* parent, Value& out,
                  std::u16string& error);

}
}
