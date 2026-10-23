#include "lfw/lfw.h"
#include "lfw/state/state_misc.h"

#include <variant>

#include "lfw/defines/entity_enum.h"
#include "lfw/defines/frame_id.h"
#include "lfw/data/frame_info.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/entity/entity.h"

namespace lfw {
namespace state {
namespace {

void swb_on_landing(Entity& e, const Value& velocity) {
  (void)velocity;
  e.enter_frame_by_id(Value(std::u16string(frame_id::kGone)));
}

void cs2_enter(Entity& e, const FrameInfo* prev_frame) {
  (void)prev_frame;
  const EntityData* d = e.lfw().datas().resolve_entity_data(e.datas_find_fighter(u"50"));
  if (d == nullptr || !d->valid()) return;
  e.transform(d);
  e.enter_frame(Value(std::u16string(frame_id::kAuto)));
}

void bsb_enter(Entity& e, const FrameInfo* prev_frame) {
  (void)prev_frame;
  const double st = e.state();
  if (st != static_cast<double>(StateEnum::Ball_Hitting) &&
      st != static_cast<double>(StateEnum::Ball_Hit) &&
      st != static_cast<double>(StateEnum::Ball_Rebounding) &&
      st != static_cast<double>(StateEnum::Ball_Disappear)) {
    return;
  }
  e.set_shaking(to_number(Value(0.0)));
  e.set_motionless(to_number(Value(0.0)));
  e.set_velocity(Value(0.0), Value(0.0), Value(0.0));
}

}

State_WeaponBroken::State_WeaponBroken(Value state) : State_Base(std::move(state)) {
  on_landing = &swb_on_landing;
}

State_TransformToCatching::State_TransformToCatching(Value state) : State_Base(std::move(state)) {}

CharacterState_TransformToLouisEX::CharacterState_TransformToLouisEX(Value state)
    : State_Base(std::move(state)) {
  enter = &cs2_enter;
}

BallState_Base::BallState_Base(Value state) : State_Base(std::move(state)) {
  enter = &bsb_enter;
}

void State_TransformToCatching::update(Entity& e) {
  e.transfrom_to_another();
  e.enter_frame(Value(std::u16string(frame_id::kAuto)));
}

void State_TransformTo8XXX::leave(Entity& e, const FrameInfo* next_frame) {
  (void)next_frame;
  if (!std::holds_alternative<double>(state())) return;
  const std::u16string oid = to_string(Value(to_number(state()) - 8000));
  const EntityData* data = e.lfw().datas().resolve_entity_data(e.datas_find(oid));
  const Value old_data = e.data()->to_value();
  if (data != nullptr && data->valid()) e.transform(data);
  e.enter_frame(Value(std::u16string(frame_id::kAuto)));
  const Value new_type = e.data_type();
  if (!strict_equals(lfw::field_or(old_data, u"type"), new_type) &&
      strict_equals(new_type, Value(static_cast<double>(EntityEnum::Fighter)))) {
    e.world_callbacks_call(u"on_fighter_add");
  }
}

}
}
