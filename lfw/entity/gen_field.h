#pragma once

#include <optional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"
#include "lfw/utils/math/mersenne_twister.h"

namespace lfw {

enum class GenClosure {
  IcePiece,
  BuringSmokeAction,
};

void register_gen_closure(const Value& holder, GenClosure kind);

/**
 * 求 opoint 上 `gen_*` 表达式 / `__gen_*` 闭包的值。
 *
 * `emitter_frame` 是发射者的当前帧（表达式里的 `w`/`h`/`cx`/`cy` 都从它上面取）。
 */
std::optional<Value> gen_field_eval(const Value& holder, const std::u16string& kind,
                                    const FrameInfo* emitter_frame, MersenneTwister& mt);

struct GenFieldStats {
  long long calls = 0;
  long long resolved = 0;
  long long closures = 0;
};

const GenFieldStats& gen_field_stats();

}
