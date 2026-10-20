#pragma once

#include "lfw/base/expression.h"
#include "lfw/core/value.h"

namespace lfw {

class Entity;

namespace loader {

bool preprocess_next_frame(Value& nf);

Expression<Entity>* judger_of(const Value& nf);

}
}
