#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"
#include "lfw/utils/math/mersenne_twister.h"

namespace lfw {
namespace state {

Value spawn_ice_piece(const std::u16string& id);

const std::vector<Value>& ice_piece_opoints();

/** 冰片初速（`__gen_dvx` / `__gen_dvy`）：只跟随机数有关。 */
Value ice_piece_dvx(MersenneTwister& mt);
Value ice_piece_dvy(MersenneTwister& mt);

/** 冰片位置（`__gen_x` / `__gen_y`）：按发射者帧的宽高随机。 */
Value ice_piece_x(const FrameInfo& frame, MersenneTwister& mt);
Value ice_piece_y(const FrameInfo& frame, MersenneTwister& mt);

}
}
