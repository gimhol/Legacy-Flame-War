#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/expression.h"
#include "lfw/collision/collision.h"
#include "lfw/entity/entity.h"

namespace lfw {

namespace loader {

const std::vector<std::pair<std::u16string, ValGetter<collision::Collision>>>&
collision_val_getters();

ValGetter<collision::Collision> get_val_getter_from_collision(const std::u16string& word);

}

}
