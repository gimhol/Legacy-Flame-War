#pragma once

#include <string>

#include "lfw/base/expression.h"
#include "lfw/loader/get_val_from_lf2.h"

namespace lfw {

namespace loader {

template <typename Ctx>
ValGetter<Ctx> get_val_from_world(const std::u16string& word) {
  return get_val_from_lf2<Ctx>(word);
}

}

}
