#pragma once

#include <optional>

#include "lfw/animation/loop.h"

namespace lfw {

class Animation {
 public:
  Animation() = default;
  virtual ~Animation() = default;

  double fill_mode() const { return _fill_mode; }
  Animation& set_fill_mode(double v);
  double count() const { return _loop.count(); }
  Animation& set_count(double v);
  double times() const { return _loop.times(); }
  Animation& set_times(double v);
  double direction() const { return _direction; }
  Animation& set_direction(double v);
  bool reverse() const { return _direction == -1; }
  Animation& set_reverse(bool v);
  double duration() const { return _duration; }
  Animation& set_duration(double v);
  double value() const { return _value; }
  Animation& set_value(double v);
  double time() const { return _time; }
  Animation& set_time(double v);
  bool done() const;
  Loop& loop() { return _loop; }
  const Loop& loop() const { return _loop; }

  virtual Animation& start(std::optional<bool> reverse = std::nullopt);
  virtual Animation& end(std::optional<bool> reverse = std::nullopt);
  virtual Animation& calc();
  Animation& update(double dt);
  Animation& auto_trip(bool reverse, double dt);

 private:
  double _value = 0;
  double _time = 0;
  double _duration = 0;
  double _direction = 1;
  double _fill_mode = 1;
  Loop _loop;
};

}
