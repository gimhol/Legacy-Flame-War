#pragma once

#include <cstddef>
#include <string>
#include <variant>

#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/data/entity_data.h"
#include "lfw/defines/entity_enum.h"
#include "lfw/defines/entity_group.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace entity {

bool is_fighter_data(const Value& v);
bool is_weapon_data(const Value& v);
bool is_entity_data(const Value& v);
bool is_ball_data(const Value& v);
inline bool is_fighter_data(const EntityData* d) { return d->is_fighter(); }
inline bool is_weapon_data(const EntityData* d) { return d->is_weapon(); }
inline bool is_entity_data(const EntityData* d) { return d->is_entity(); }
inline bool is_ball_data(const EntityData* d) { return d->is_ball(); }
bool is_fighter(const Entity& e);
bool is_ball(const Entity& e);
bool is_weapon(const Entity& e);
bool is_entity(const Value& v);
bool is_boss(const Entity& e);
bool is_object(const Value& v);
bool is_object_data(const Value& v);
bool is_bg_data(const Value& v);
bool is_bot_ctrl(const controller::BaseController* ctrl);

}
}
