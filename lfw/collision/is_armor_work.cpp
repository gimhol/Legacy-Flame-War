#include "lfw/collision/is_armor_work.h"

#include "lfw/collision/collision.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/itr_effect.h"
#include "lfw/defines/state_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace collision {
namespace {

bool state_is(const FrameInfo* v, StateEnum want) {
  return v->state() == static_cast<double>(want);
}

bool effect_is(const Value& effect, ItrEffect want) {
  return strict_equals(effect, Value(static_cast<double>(want)));
}

bool state_in_any(const FrameInfo* v, StateEnum a, StateEnum b, StateEnum c, StateEnum d,
                  StateEnum e) {
  return state_is(v, a) || state_is(v, b) || state_is(v, c) || state_is(v, d) || state_is(v, e);
}

}

bool is_armor_work(const Collision& c) {
  const Entity* const victim = c.victim.entity();
  const ArmorInfo& armor = victim != nullptr ? victim->armor : ArmorInfo();
  if (!armor.valid()) return false;

  const FrameInfo* bframe = c.bframe;
  if (state_is(bframe, StateEnum::Caught) || state_is(bframe, StateEnum::Injured) ||
      state_is(bframe, StateEnum::Falling) || state_is(bframe, StateEnum::Frozen) ||
      state_is(bframe, StateEnum::Lying) || state_is(bframe, StateEnum::Tired) ||
      state_is(bframe, StateEnum::BrokenDefend) || state_is(bframe, StateEnum::Burning)) {
    return false;
  }

  if (armor.fulltime().has_value() && !*armor.fulltime() &&
      !state_in_any(bframe, StateEnum::Standing, StateEnum::Walking, StateEnum::Running,
                    StateEnum::Jump, StateEnum::Dash)) {
    return false;
  }

  const Value& itr = c.itr;
  const Value effect = field_or(itr, u"effect");

  if (!truthy(Value(armor.fireproof())) &&
      (effect_is(effect, ItrEffect::Fire) || effect_is(effect, ItrEffect::MFire1) ||
       effect_is(effect, ItrEffect::MFire2) || effect_is(effect, ItrEffect::FireExplosion))) {
    return false;
  }

  if (!truthy(Value(armor.antifreeze())) &&
      (effect_is(effect, ItrEffect::Ice2) || effect_is(effect, ItrEffect::Ice))) {
    return false;
  }

  if (ge(field_or(itr, u"bdefend"),
         Value(defines::num(u"Defines.DEFAULT_FORCE_BREAK_DEFEND_VALUE")))) {
    return false;
  }

  if (state_is(c.aframe, StateEnum::Ball_3006)) return false;

  return true;
}

}
}
