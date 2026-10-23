#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/buff/buff.h"
#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"
#include "lfw/entity/drink_info.h"

namespace lfw {

class Entity;

namespace state {


class State_Base {
 public:
  explicit State_Base(Value state) : _state(std::move(state)) {}
  virtual ~State_Base() = default;

  const Value& state() const { return _state; }

  std::function<void(Entity& e)> pre_update;
  /** `prev_frame` = 换状态前的那一帧。 */
  std::function<void(Entity& e, const FrameInfo* prev_frame)> enter;
  std::function<void(Entity& e)> on_dead;
  std::function<void(Entity& e, const Value& velocity)> on_landing;
  std::function<Value(Entity& e)> get_gravity;
  std::function<Value(Entity& e)> get_sudden_death_frame;
  std::function<Value(Entity& e)> get_caught_end_frame;
  std::function<const FrameInfo*(Entity& e)> get_auto_frame;
  std::function<const FrameInfo*(Entity& e, const Value& id)> find_frame_by_id;
  std::function<void(Entity& e)> on_leave_ground;

  virtual void update(Entity& e);
  /** `next_frame` = 离开这一状态时已经在的那一帧。 */
  virtual void leave(Entity& e, const FrameInfo* next_frame);
  virtual void on_restrict(Entity& e, double x, double y, double z);

 protected:
  Value _state;
};

}
}
