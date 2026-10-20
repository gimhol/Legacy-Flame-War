#pragma once

#include <optional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/utils/math/mersenne_twister.h"

namespace lfw {

enum class GenClosure {
  IcePiece,
  BuringSmokeAction,
};

void register_gen_closure(const Value& holder, GenClosure kind);

std::optional<Value> gen_field_eval(const Value& holder, const std::u16string& kind,
                                    const Value& emitter_frame, const Value& emitter_data,
                                    MersenneTwister& mt);

struct GenFieldStats {
  long long calls = 0;
  long long resolved = 0;
  long long closures = 0;
};

const GenFieldStats& gen_field_stats();

}
