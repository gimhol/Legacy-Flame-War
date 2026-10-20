#pragma once

#include "lfw/animation/periodic.h"

namespace lfw {

class Tangent : public Periodic {
 public:
  using Periodic::Periodic;

  double method(double v) override;
};

}
