#pragma once

namespace lfw {

class Loop {
 public:
  Loop& set_count(double v);
  Loop& set_times(double v);
  Loop& set(double count, double times);
  Loop& reset();
  bool continue_();
  bool done() const;

  double count() const { return _count; }
  double times() const { return _times; }

 private:
  double _times = 1;
  double _count = 0;
};

}
