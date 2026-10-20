#pragma once

#include <string>
#include <utility>
#include <vector>

#include "lfw/base/expression.h"
#include "lfw/entity/entity.h"

namespace lfw {

namespace loader {

const std::vector<std::pair<std::u16string, ValGetter<Entity>>>& entity_val_getters();

ValGetter<Entity> get_val_getter_from_entity(const std::u16string& word);

}

}
