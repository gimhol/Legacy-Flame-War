#include "lfw/bot/bot_data.h"

#include <utility>

#include "lfw/base/expression.h"
#include "lfw/bot/bot_controller.h"
#include "lfw/defines/defines_data.h"
#include "lfw/loader/get_val_from_bot_ctrl.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/type_cast.h"

namespace lfw {
namespace bot {
namespace {

bool undefined_v(const Value& v) { return std::holds_alternative<std::monostate>(v); }

double num_or(const Value& holder, const char16_t* key, double fallback) {
  const Value v = field_or(holder, key);
  return undefined_v(v) ? fallback : to_number(v);
}

BotRay ray_of(const Value& raw) {
  BotRay ray;
  ray.x = num_or(raw, u"x", 0.0);
  ray.z = num_or(raw, u"z", 0.0);
  ray.min_x = num_or(raw, u"min_x", 0.0);
  ray.max_x = num_or(raw, u"max_x", 10000.0);
  ray.min_z = num_or(raw, u"min_z", 0.0);
  ray.max_z = num_or(raw, u"max_z", 10000.0);
  const Value max_d = field_or(raw, u"max_d");
  ray.has_max_d = !undefined_v(max_d);
  ray.max_d = ray.has_max_d ? to_number(max_d) : 0.0;
  ray.reverse = truthy(field_or(raw, u"reverse"));
  return ray;
}

std::vector<std::u16string> strings_of(const Value& array_value) {
  std::vector<std::u16string> out;
  const Array* const arr = as_array(array_value);
  if (arr == nullptr) return out;
  for (size_t i = 0; i < arr->size(); ++i) out.push_back(to_string(arr->at(i)));
  return out;
}

std::vector<std::u16string> strings_of(const Value& holder, const char16_t* key,
                                      bool* present) {
  const Value v = field_or(holder, key);
  *present = as_array(v) != nullptr;
  return strings_of(v);
}

/** `{ [id]: string[] }` 这类映射表。 */
std::map<std::u16string, std::vector<std::u16string>> id_lists_of(const Value& holder) {
  std::map<std::u16string, std::vector<std::u16string>> out;
  const Object* const o = as_object(holder);
  if (o == nullptr) return out;
  for (const std::u16string& key : o->keys()) {
    const Value* const v = o->get(key);
    if (v == nullptr) continue;
    out[key] = strings_of(*v);
  }
  return out;
}

}

BotAction::BotAction(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _action_id = to_string(field_or(*o, u"action_id"));

  const Value desire = field_or(*o, u"desire");
  _has_desire = !undefined_v(desire);
  _desire = _has_desire ? to_number(desire) : 0.0;
  _desire_base = num_or(_raw, u"desire_base", 0.0);
  _desire_step = num_or(_raw, u"desire_step", 0.0);

  const Value rays = field_or(*o, u"e_ray");
  _has_e_ray = truthy(rays);
  const Array* const ray_arr = as_array(rays);
  if (ray_arr != nullptr) {
    for (size_t i = 0; i < ray_arr->size(); ++i) _e_ray.push_back(ray_of(ray_arr->at(i)));
  }

  _status = strings_of(_raw, u"status", &_has_status);
  _keys = field_or(*o, u"keys");
  _frame = field_or(*o, u"frame");

  const Value judge = field_or(*o, u"expression");
  if (truthy(judge)) {
    _judger = std::make_shared<Expression<BotController>>(to_string(judge),
                                                          loader::get_val_from_bot_ctrl);
  }
}

BotData::BotData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = to_string(field_or(*o, u"id"));
  _oid = to_string(field_or(*o, u"oid"));

  const Value actions = field_or(*o, u"actions");
  if (const Object* const action_o = as_object(actions)) {
    for (const std::u16string& key : action_o->keys()) {
      const Value* const v = action_o->get(key);
      if (v == nullptr) continue;
      _actions.emplace(key, BotAction(*v));
    }
  }

  _states = id_lists_of(field_or(*o, u"states"));
  _frames = id_lists_of(field_or(*o, u"frames"));
  _dataset.assign(field_or(*o, u"dataset"));
}

const BotAction* BotData::action(const std::u16string& action_id) const {
  const auto it = _actions.find(action_id);
  return it != _actions.end() ? &it->second : nullptr;
}

const std::vector<std::u16string>* BotData::actions_of_state(const std::u16string& state) const {
  const auto it = _states.find(state);
  return it != _states.end() ? &it->second : nullptr;
}

const std::vector<std::u16string>* BotData::actions_of_frame(
    const std::u16string& frame_id) const {
  const auto it = _frames.find(frame_id);
  return it != _frames.end() ? &it->second : nullptr;
}

}
}
