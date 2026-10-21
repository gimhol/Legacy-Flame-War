#include "lfw/collision/is_fall.h"

#include "lfw/collision/collision.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/state_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"

namespace lfw {
namespace collision {
namespace {

bool state_is(const Entity& e, StateEnum want) {
  return strict_equals(e.state(), Value(static_cast<double>(want)));
}

}

bool is_fall(const Collision& c) {
  const Entity* const victim = c.victim.entity();
  if (victim == nullptr || !entity::is_fighter(*victim)) return true;
  const double fall_value = victim->fall_value();
  if (fall_value <= 0.0) return true;
  if (victim->hp() <= 0.0) return true;
  if (state_is(*victim, StateEnum::Frozen)) return true;
  if (fall_value < defines::num(u"Defines.DEFAULT_FALL_VALUE_DIZZY") &&
      state_is(*victim, StateEnum::Caught)) {
    return true;
  }
  if (fall_value < defines::num(u"Defines.DEFAULT_FALL_VALUE_CRITICAL") &&
      !victim->is_on_ground) {
    return true;
  }
  return false;
}

}
}
