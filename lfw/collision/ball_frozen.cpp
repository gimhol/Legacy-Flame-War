#include "lfw/collision/ball_frozen.h"

#include <cstddef>
#include <memory>
#include <variant>

#include "lfw/defines/entity_group.h"
#include "lfw/defines/frame_id.h"
#include "lfw/data/frame_info.h"
#include "lfw/defines/itr_kind.h"
#include "lfw/defines/oid.h"
#include "lfw/defines/state_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/entity/face_helper.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"

namespace lfw {
namespace collision {
namespace {

bool group_length_truthy(const Value& group) {
  if (const Array* arr = as_array(group)) return arr->size() != 0;
  if (const std::u16string* s = std::get_if<std::u16string>(&group)) return s->size() != 0;
  return false;
}

bool group_some_equal(const Value& group, const char16_t* name) {
  const Array* arr = as_array(group);
  if (arr == nullptr) return false;
  const Value want{std::u16string(name)};
  for (size_t i = 0; i < arr->size(); ++i) {
    if (equals(arr->at(i), want)) return true;
  }
  return false;
}

bool group_some_not_equal(const Value& group, const char16_t* name) {
  const Array* arr = as_array(group);
  if (arr == nullptr) return false;
  const Value want{std::u16string(name)};
  for (size_t i = 0; i < arr->size(); ++i) {
    if (!equals(arr->at(i), want)) return true;
  }
  return false;
}

Value& freeze_ball_opoint() {
  static Value v = [] {
    Object action;
    action.set(u"id", Value(std::u16string(frame_id::kAuto)));
    Object o;
    o.set(u"oid", Value(std::u16string(oid::kFreezeBall)));
    o.set(u"kind", Value(0.0));
    o.set(u"x", Value(0.0));
    o.set(u"y", Value(0.0));
    o.set(u"action", Value(std::make_shared<Object>(action)));
    o.set(u"z", Value(0.0));
    return Value(std::make_shared<Object>(o));
  }();
  return v;
}

}

bool handle_ball_frozen(Entity& a_ref, Entity& v_ref, const ItrInfo& itr) {
  Entity* a = &a_ref;
  Entity* v = &v_ref;

  if (!group_length_truthy(a->group())) return false;
  if (!group_length_truthy(v->group())) return false;

  const double itr_kind = itr.has_kind() ? itr.kind() : 0.0;
  const bool ok = itr_kind == static_cast<double>(ItrKind::Normal) ||
                  itr_kind == static_cast<double>(ItrKind::CharacterThrew) ||
                  itr_kind == static_cast<double>(ItrKind::WeaponSwing);
  if (!ok) return false;

  const Value a_group = a->group();
  const Value v_group = v->group();
  if (group_some_equal(v_group, entity_group::kFreezer) &&
      group_some_equal(a_group, entity_group::kFreezableBall)) {
    Entity* temp = a;
    a = v;
    v = temp;
  } else if (group_some_not_equal(a_group, entity_group::kFreezer) ||
             group_some_not_equal(v_group, entity_group::kFreezableBall)) {
    return false;
  }

  if (v->state() != static_cast<double>(StateEnum::Ball_Flying)) return false;
  if (!entity::is_ball_data(v->data())) {
    if (!entity::is_fighter_data(a->data())) {
      if (a->state() != static_cast<double>(StateEnum::Weapon_OnHand)) return false;
    }
  }

  const double x1 = a->position.x;
  const double y1 = a->position.y;
  const double z1 = a->position.z;
  const double x2 = v->position.x;
  const double y2 = v->position.y;
  const double z2 = v->position.z;

  const FrameInfo* a_frame = a->frame;
  const FrameInfo* v_frame = v->frame;
  const double a_centerx = a_frame->centerx();
  const double a_centery = a_frame->centery();
  const double v_centerx = v_frame->centerx();
  const double v_centery = v_frame->centery();
  const double v_width = v_frame->width();
  const double v_height = v_frame->height();

  const double a_facing = a->facing;
  const double v_facing_number = v->facing;

  const double cy1 = y1 + a_centery;
  const double cx1 = a_facing > 0 ? x1 - a_centerx : x1 + a_centerx - a_centerx;

  const double cy2 = (y2 + v_centery) - v_height / 2.0;
  const double cx2 = v_facing_number > 0 ? (x2 - v_centerx) + v_width / 2.0
                                         : (x2 + v_centerx - v_width) + v_width / 2.0;

  Value& opoint_v = freeze_ball_opoint();
  Object* opoint = as_object(opoint_v);
  opoint->set(u"x", Value(a_facing * round(cx2 - cx1)));
  opoint->set(u"y", Value(round(cy1 - cy2)));
  opoint->set(u"z", Value(round(z2 - z1)));

  Vector3 zero{};
  const bool freeze_ball =
      a->spawn(opoint_v, zero, to_number(entity::turn_face(Value(v->facing)))) != nullptr;
  if (!freeze_ball) return false;
  v->enter_frame_by_id(Value(std::u16string(frame_id::kGone)));
  return true;
}

}
}
