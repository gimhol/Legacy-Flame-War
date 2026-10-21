#pragma once

#include "lfw/core/value.h"
#include "lfw/data/bg_info.h"

namespace lfw {

class Background;

class Layer {
 public:
  Layer(Background* bg, BgLayerInfo info, double data_index = 0.0, double loop_index = -1.0);

  Background* bg() const { return _bg; }
  const BgLayerInfo& info() const { return _info; }
  double data_index() const { return _data_index; }
  double loop_index() const { return _loop_index; }

  bool visible() const { return _visible; }
  void set_visible(bool v) { _visible = v; }

  bool is_static() const;
  void update(double count);

 private:
  Background* _bg = nullptr;
  BgLayerInfo _info;
  double _data_index = 0.0;
  double _loop_index = -1.0;
  bool _visible = false;
};

}
