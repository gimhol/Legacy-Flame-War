#include "lfw/bot/bot_dataset.h"

#include <variant>

#include "lfw/defines/defines_data.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace bot {
namespace {

bool undefined_v(const Value& v) { return std::holds_alternative<std::monostate>(v); }

double num_or(const Value& src, const char16_t* key, double cur) {
  const Value v = field_or(src, key);
  return undefined_v(v) ? cur : to_number(v);
}

}

BotDataSet::BotDataSet() {
  const double max_desire = defines::num(u"Defines.MAX_AI_DESIRE");
  r_atk_desire = max_desire;
  j_atk_desire = max_desire;
}

const BotDataSet& BotDataSet::Default() {
  static const BotDataSet kDefault;
  return kDefault;
}

void BotDataSet::assign(const Value& src) {
  if (as_object(src) == nullptr) return;
  w_atk_x = num_or(src, u"w_atk_x", w_atk_x);
  w_atk_m_x = num_or(src, u"w_atk_m_x", w_atk_m_x);
  w_atk_r_x = num_or(src, u"w_atk_r_x", w_atk_r_x);
  w_atk_min_z = num_or(src, u"w_atk_min_z", w_atk_min_z);
  w_atk_max_z = num_or(src, u"w_atk_max_z", w_atk_max_z);
  r_atk_desire = num_or(src, u"r_atk_desire", r_atk_desire);
  j_atk_desire = num_or(src, u"j_atk_desire", j_atk_desire);
  r_atk_x = num_or(src, u"r_atk_x", r_atk_x);
  r_atk_min_z = num_or(src, u"r_atk_min_z", r_atk_min_z);
  r_atk_max_z = num_or(src, u"r_atk_max_z", r_atk_max_z);
  d_atk_min_x = num_or(src, u"d_atk_min_x", d_atk_min_x);
  d_atk_max_x = num_or(src, u"d_atk_max_x", d_atk_max_x);
  d_atk_min_z = num_or(src, u"d_atk_min_z", d_atk_min_z);
  d_atk_max_z = num_or(src, u"d_atk_max_z", d_atk_max_z);
  j_atk_x = num_or(src, u"j_atk_x", j_atk_x);
  j_atk_min_z = num_or(src, u"j_atk_min_z", j_atk_min_z);
  j_atk_max_z = num_or(src, u"j_atk_max_z", j_atk_max_z);
  j_atk_min_y = num_or(src, u"j_atk_min_y", j_atk_min_y);
  j_atk_max_y = num_or(src, u"j_atk_max_y", j_atk_max_y);
  jump_desire = num_or(src, u"jump_desire", jump_desire);
  dash_desire = num_or(src, u"dash_desire", dash_desire);
  r_desire_min = num_or(src, u"r_desire_min", r_desire_min);
  r_desire_max = num_or(src, u"r_desire_max", r_desire_max);
  r_x_min = num_or(src, u"r_x_min", r_x_min);
  r_x_max = num_or(src, u"r_x_max", r_x_max);
  r_stop_desire = num_or(src, u"r_stop_desire", r_stop_desire);
  defend_desire_base = num_or(src, u"defend_desire_base", defend_desire_base);
  defend_desire_step = num_or(src, u"defend_desire_step", defend_desire_step);
  avoid_in_x = num_or(src, u"avoid_in_x", avoid_in_x);
  avoid_in_z = num_or(src, u"avoid_in_z", avoid_in_z);
  avoid_out_x = num_or(src, u"avoid_out_x", avoid_out_x);
  avoid_out_z = num_or(src, u"avoid_out_z", avoid_out_z);
  pick_weapon_f_x = num_or(src, u"pick_weapon_f_x", pick_weapon_f_x);
  pick_weapon_b_x = num_or(src, u"pick_weapon_b_x", pick_weapon_b_x);
  pick_weapon_z = num_or(src, u"pick_weapon_z", pick_weapon_z);
}

void BotDataSet::reset() { *this = Default(); }

}
}
