#pragma once

#include "lfw/animation/animation.h"

namespace lfw {

class Delay : public Animation {
 public:
  explicit Delay(double value);
  Animation& calc() override;
};

}
