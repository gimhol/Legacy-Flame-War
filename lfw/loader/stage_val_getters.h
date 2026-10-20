#pragma once

#include <string>

#include "lfw/base/expression.h"

namespace lfw {

namespace stage {
class Stage;
}

namespace loader {

ValGetter<stage::Stage> get_val_getter_from_stage(const std::u16string& word);

}
}
