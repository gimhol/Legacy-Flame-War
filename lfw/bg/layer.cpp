#include "lfw/bg/layer.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace {

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

}

Layer::Layer(Background* bg, BgLayerInfo info, double data_index, double loop_index)
    : _bg(bg),
      _info(std::move(info)),
      _data_index(data_index),
      _loop_index(loop_index) {}

bool Layer::is_static() const {
  const bool no_cycle = !_info.has(u"cc") || !_info.has(u"c1") || !_info.has(u"c2");
  return no_cycle && !truthy(Value(_info.offsetAnimX())) &&
         !truthy(Value(_info.offsetAnimY())) && truthy(Value(_info.absolute()));
}

void Layer::update(double count) {
  if (_info.has(u"cc") && _info.has(u"c1") && _info.has(u"c2")) {
    const double now = std::fmod(count, _info.cc());
    _visible = now >= _info.c1() && now <= _info.c2();
  } else {
    _visible = true;
  }
}

}
