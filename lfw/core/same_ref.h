#pragma once

#include <memory>

#include "lfw/core/value.h"

namespace lfw {

inline bool same_ref(const Value& a, const Value& b) {
  const auto* pa = std::get_if<std::shared_ptr<Object>>(&a);
  const auto* pb = std::get_if<std::shared_ptr<Object>>(&b);
  if (pa != nullptr && pb != nullptr) return *pa == *pb;
  const auto* aa = std::get_if<std::shared_ptr<Array>>(&a);
  const auto* ab = std::get_if<std::shared_ptr<Array>>(&b);
  if (aa != nullptr && ab != nullptr) return *aa == *ab;
  return false;
}

}
