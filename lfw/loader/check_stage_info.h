#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace loader {

bool check_stage_info(const Value& info, std::vector<std::u16string>* errors = nullptr);
bool check_phase_info(const Value& stage, const Value& info, int idx,
                      std::vector<std::u16string>* errors = nullptr);

}
}
