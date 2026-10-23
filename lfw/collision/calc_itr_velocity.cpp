#include "lfw/collision/calc_itr_velocity.h"

#include <variant>

#include "lfw/collision/collision.h"
#include "lfw/collision/is_fall.h"
#include "lfw/core/value.h"
#include "lfw/defines/itr_effect.h"
#include "lfw/defines/state_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/type_cast.h"

namespace lfw {
namespace collision {
namespace {

bool nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

double number_or(const Value& v, double fallback) {
  return std::holds_alternative<std::monostate>(v) ? fallback : to_number(v);
}

Value dataset_of(const Entity* const e, const char16_t* name) {
  return e != nullptr ? e->dataset(name) : Value();
}

double weight_of(const Entity* const victim) {
  const Value w = victim != nullptr ? field_or(victim->data()->base(), u"weight") : Value();
  if (nullish(w)) return 1.0;
  return to_number(w);
}

double x_of(const Entity* const e) {
  return e != nullptr ? e->position.x : to_number(Value());
}

}

ItrVelocity calc_itr_velocity(const Collision& c) {
  const Entity* const attacker = c.attacker.entity();
  const Entity* const victim = c.victim.entity();
  const Value& itr = c.itr->to_value();

  const double dvx = number_or(field_or(itr, u"dvx"), 0.0);
  const Value dvy_raw = field_or(itr, u"dvy");
  const double dvy = std::holds_alternative<std::monostate>(dvy_raw)
                         ? to_number(dataset_of(attacker, u"ivy_d"))
                         : to_number(dvy_raw);
  const double dvz = number_or(field_or(itr, u"dvz"), 0.0);

  const double diff_x = x_of(victim) - x_of(attacker);
  const double weight = weight_of(victim);

  const bool position_based =
      equals(field_or(itr, u"effect"), Value(static_cast<double>(ItrEffect::FireExplosion))) ||
      equals(field_or(itr, u"effect"), Value(static_cast<double>(ItrEffect::Explosion))) ||
      (attacker != nullptr &&
       strict_equals(attacker->state(),
                     Value(static_cast<double>(StateEnum::HeavyWeapon_InTheSky))));

  Value x_direction(-1.0);
  if (!position_based) {
    x_direction = attacker != nullptr ? Value(attacker->facing) : Value();
  } else if (diff_x > 0.0) {
    x_direction = Value(-1.0);
  } else if (diff_x < 0.0) {
    x_direction = Value(1.0);
  }

  ItrVelocity out{};
  out.x = dvx * to_number(dataset_of(attacker, u"ivx_f")) * to_number(x_direction) / weight;
  out.y = (is_fall(c) ? dvy * to_number(dataset_of(attacker, u"ivy_f")) : 0.0) / weight;
  out.z = dvz * to_number(dataset_of(attacker, u"ivz_f")) / weight;
  out.x_direction = x_direction;
  return out;
}

}
}
