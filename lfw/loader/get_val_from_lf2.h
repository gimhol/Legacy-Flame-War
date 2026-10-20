#pragma once

#include <string>

#include "lfw/base/expression.h"

namespace lfw {

namespace loader {

template <typename Ctx>
ValGetter<Ctx> get_val_from_lf2(const std::u16string& word) {
  (void)word;
  return nullptr;
}

}

}
