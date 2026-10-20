#pragma once

#include "lfw/core/value.h"

namespace lfw {
namespace schema {

Value make_schema(const Value& meta, const Value* parent = nullptr);

}
}
