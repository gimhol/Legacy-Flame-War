#pragma once

#include "lfw/core/value.h"

namespace lfw::ui {

struct CrossInfo {
  double left = 0;
  double top = 0;
  double right = 0;
  double bottom = 0;
  double mid_x = 0;
  double mid_y = 0;

  static CrossInfo make(const Value& o);
  void set(const Value& o);
  bool compare(const CrossInfo& o) const;
  CrossInfo clone() const { return *this; }
};

}
