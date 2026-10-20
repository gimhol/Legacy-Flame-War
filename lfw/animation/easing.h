#pragma once

#include <functional>

#include "lfw/animation/animation.h"

namespace lfw {

using IEasing = std::function<double(double factor, double val_1, double val_2)>;

class Easing : public Animation {
 public:
  explicit Easing(double begin = 0, double end = 1);

  Easing& set(double begin, double end);
  Easing& set_easing(IEasing v);

  double val_1() const { return _val_1; }
  double val_2() const { return _val_2; }
  void set_val_1(double v) { _val_1 = v; }
  void set_val_2(double v) { _val_2 = v; }
  const IEasing& easing() const { return _easing; }

  Animation& calc() override;

 private:
  double _val_1 = 0;
  double _val_2 = 1;
  IEasing _easing;
};

}
