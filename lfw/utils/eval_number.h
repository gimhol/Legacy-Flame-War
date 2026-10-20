#pragma once

#include <map>
#include <optional>
#include <string>

#include "lfw/core/value.h"

namespace lfw {
namespace utils {

using EvalContext = std::map<std::u16string, double>;

std::optional<double> eval_number(const Value& value, const EvalContext& ctx);

}
}
