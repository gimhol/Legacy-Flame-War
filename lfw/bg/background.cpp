#include "lfw/bg/background.h"

#include <memory>
#include <utility>

#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"

namespace lfw {
namespace {

bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

double num_or(const Value& v, double or_value) {
  return is_nullish(v) ? or_value : to_number(v);
}

Value with_x(const Value& info, double x) {
  auto out = std::make_shared<Object>();
  if (const Object* o = as_object(info)) {
    for (const std::u16string& key : o->keys()) {
      const Value* v = o->get(key);
      out->set(key, v != nullptr ? *v : Value());
    }
  }
  out->set(u"x", Value(x));
  return Value(out);
}

}

Background::Background(World* world, const BackgroundData& data)
    : _world(world), _data(data) {
  _id = Value(_data.id());

  const BgInfo& info = _data.base();

  _name = info.name().empty() ? _id : Value(info.name());

  _left = num_or(Value(info.left()), 0.0);
  _right = num_or(Value(info.right()), 0.0);
  _near = num_or(Value(info.near()), 0.0);
  _far = num_or(Value(info.far()), 0.0);
  _width = _right - _left;
  _depth = _near - _far;
  _height = num_or(Value(info.height()), 0.0);
  _middle.x = (_right + _left) / 2.0;
  _middle.z = (_far + _near) / 2.0;

  for (const BgLayerInfo& layer : _data.layers()) add_layer(layer);

  _zoom_x = num_or(Value(info.zoom_x()), 1.0);
  _zoom_y = num_or(Value(info.zoom_y()), 1.0);
  _zoom_z = num_or(Value(info.zoom_z()), 1.0);
}

void Background::add_layer(const BgLayerInfo& info) {
  const double data_index = _layer_data_index++;
  double x = to_number(Value(info.x()));
  const double loop = info.has(u"loop") ? to_number(Value(info.loop())) : 0.0;
  if (loop <= 0.0) {
    _layers.push_back(std::make_shared<Layer>(this, info, data_index, -1.0));
    return;
  }
  const double right = _width + loop;
  double loop_index = 0.0;
  for (x -= loop; x < right; x += loop) {
    _layers.push_back(std::make_shared<Layer>(this, info.with_x(x), data_index, loop_index));
    loop_index++;
  }
}

void Background::update() {
  _update_times++;
  for (const std::shared_ptr<Layer>& layer : _layers) layer->update(_update_times);
}

void Background::dispose() {
  _layers.clear();
  _layer_data_index = 0.0;
}

}
