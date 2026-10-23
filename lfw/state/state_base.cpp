#include "lfw/state/state_base.h"

#include <variant>

#include "lfw/buff/buff_healing.h"
#include "lfw/defines/state_enum.h"
#include "lfw/utils/math/clamp.h"
#include "lfw/utils/math/float_equal.h"
#include "lfw/entity/entity.h"

namespace lfw {
namespace state {
namespace {

constexpr double kMinV = 0.5;
constexpr double kStateHealSelfHp = 104;

bool is_null(const Value& v) { return std::holds_alternative<NullTag>(v); }

Value nan_to_null(double v) { return std::isnan(v) ? Value() : Value(v); }

Value clamp_velocity(const Value& v) {
  const double d = to_number(v);
  if (d < -kMinV) return Value(-kMinV);
  if (d > kMinV) return Value(kMinV);
  return v;
}

double clamp_velocity(double v) {
  if (v < -kMinV) return -kMinV;
  if (v > kMinV) return kMinV;
  return v;
}

}

void State_Base::update(Entity& e) { (void)e; }

void State_Base::leave(Entity& e, const FrameInfo* next_frame) {
  (void)next_frame;
  if (!strict_equals(_state, Value(static_cast<double>(StateEnum::HealSelf)))) return;
  Entity* const victim = &e;
  const double duration =
      victim == nullptr ? 0.0 : buff::Buff_Healing::duration_of(*victim, kStateHealSelfHp);
  buff::grant_buff(buff::Buff_Healing::KIND, nullptr, victim, duration);
}

void State_Base::on_restrict(Entity& e, double x, double y, double z) {
  Value vx(NullTag{});
  Value vz(NullTag{});
  const Value vy(NullTag{});
  double px = 0;
  double py = 0;
  double pz = 0;
  px = e.position.x; py = e.position.y; pz = e.position.z;
  if (!float_equal(x, px)) vx = nan_to_null(clamp_velocity(e.velocity_x()));
  if (!float_equal(z, pz)) vz = nan_to_null(clamp_velocity(e.velocity_z()));
  if (!float_equal(y, py)) {
    vx = nan_to_null(clamp_velocity(e.velocity_x()));
    vz = nan_to_null(clamp_velocity(e.velocity_z()));
  }
  if (!is_null(vx) || !is_null(vz) || !is_null(vy)) e.set_velocity(vx, vy, vz);
  e.assign_position(x, y, z);
}

}
}
