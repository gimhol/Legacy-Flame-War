#pragma once

#include "lfw/animation/animation.h"

namespace lfw {

class Periodic : public Animation {
 public:
  explicit Periodic(double bottom = 0, double height = 1, double scale = 1);

  double offset = 0;

  Periodic& set_offset(double v);
  Periodic& set_scale(double v);
  Periodic& set(double bottom, double height, double scale);

  double bottom() const { return _b; }
  void set_bottom(double v);
  double height() const { return _h; }
  void set_height(double v);
  double scale() const { return _s; }

  virtual double method(double v) = 0;
  Animation& calc() override;

 private:
  double _b = 0;
  double _h = 1;
  double _s = 1;
};

}
