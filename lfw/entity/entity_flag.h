#pragma once

#include <cstdint>
#include <string>

#include "lfw/core/js_num.h"
#include "lfw/defines/hit_flag.h"

namespace lfw {

inline double flag_between(const std::u16string& a_team, double a_hp, double a_type,
                           const std::u16string& b_team) {
  int32_t ret = a_team == b_team ? static_cast<int32_t>(HitFlag::Ally)
                                 : static_cast<int32_t>(HitFlag::Enemy);
  if (a_hp <= 0) ret |= static_cast<int32_t>(HitFlag::Dead);
  return static_cast<double>(ret | js_to_int32(a_type));
}

}
