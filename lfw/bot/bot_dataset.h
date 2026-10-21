#pragma once

#include "lfw/core/value.h"

namespace lfw {
namespace bot {

/** `IBotDataSet`：BOT 的各项距离/欲望阈值。加载期逐字段解析，运行期直接读。 */
class BotDataSet {
 public:
  BotDataSet();

  static const BotDataSet& Default();

  /** 只覆盖 `src` 里出现的字段（其它保持当前值）。 */
  void assign(const Value& src);
  void reset();

    double w_atk_x = 50;
    double w_atk_m_x = -1;
    double w_atk_r_x = -1;
    double w_atk_min_z = -15;
    double w_atk_max_z = 15;
    double r_atk_desire = 0;   // 构造时取 Defines.MAX_AI_DESIRE
    double j_atk_desire = 0;   // 同上
    double r_atk_x = 100;
    double r_atk_min_z = -15;
    double r_atk_max_z = 15;
    double d_atk_min_x = 0;
    double d_atk_max_x = 100;
    double d_atk_min_z = -60;
    double d_atk_max_z = 60;
    double j_atk_x = 180;
    double j_atk_min_z = -60;
    double j_atk_max_z = 60;
    double j_atk_min_y = -260;
    double j_atk_max_y = 260;
    double jump_desire = 50;
    double dash_desire = 100;
    double r_desire_min = 0;
    double r_desire_max = 2000;
    double r_x_min = 100;
    double r_x_max = 1200;
    double r_stop_desire = 10;
    double defend_desire_base = 0;
    double defend_desire_step = 1800;
    double avoid_in_x = 180;
    double avoid_in_z = 100;
    double avoid_out_x = 200;
    double avoid_out_z = 200;
    double pick_weapon_f_x = 25;
    double pick_weapon_b_x = -25;
    double pick_weapon_z = 25;

};

}
}
