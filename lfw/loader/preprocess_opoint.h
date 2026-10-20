#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/val_expression.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/state_enum.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

using ValWarn = std::function<void(const std::u16string&)>;

namespace opoint_detail {

inline bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

inline bool state_is_ball_default(const Value& state) {
  const double* const d = std::get_if<double>(&state);
  if (d == nullptr) return false;
  return *d == static_cast<double>(StateEnum::Ball_Flying) ||
         *d == static_cast<double>(StateEnum::Ball_3006) ||
         *d == static_cast<double>(StateEnum::Weapon_Throwing) ||
         *d == static_cast<double>(StateEnum::HeavyWeapon_InTheSky);
}

}

inline void opoint_apply_speedz_default(Object& opoint, const Value& frame_state) {
  if (!opoint_detail::state_is_ball_default(frame_state)) return;
  const Value* const speedz = opoint.get(u"speedz");
  if (speedz != nullptr && !opoint_detail::is_nullish(*speedz)) return;
  opoint.set(u"speedz", Value(defines::num(u"Defines.DEFAULT_OPOINT_SPEED_Z")));
}

inline const std::vector<std::pair<std::u16string, std::u16string>>& opoint_gen_fields() {
  static const std::vector<std::pair<std::u16string, std::u16string>> kFields{
      {u"gen_x", u"__gen_x"},
      {u"gen_y", u"__gen_y"},
      {u"gen_z", u"__gen_z"},
      {u"gen_dvx", u"__gen_dvx"},
      {u"gen_dvy", u"__gen_dvy"},
      {u"gen_dvz", u"__gen_dvz"},
      {u"gen_spread_x", u"__gen_spread_x"},
      {u"gen_spread_y", u"__gen_spread_y"},
      {u"gen_spread_z", u"__gen_spread_z"},
  };
  return kFields;
}

template <typename Ctx>
std::map<std::u16string, ValExpression<Ctx>> preprocess_opoint(Object& opoint, const Value& data,
                                                               const Value& frame,
                                                               const ValWarn& warn = {}) {
  std::map<std::u16string, ValExpression<Ctx>> out;
  for (const std::pair<std::u16string, std::u16string>& field : opoint_gen_fields()) {
    const Value* src = opoint.get(field.first);
    if (src == nullptr || !truthy(*src)) continue;
    const std::u16string* text = std::get_if<std::u16string>(src);
    if (text == nullptr) continue;
    ValExpressionOptions<Ctx> options;
    options.tag = field.first;
    ValExpression<Ctx> expr(*text, options);
    if (expr.has_err) {
      if (warn) warn(expr.err);
      continue;
    }
    out.emplace(field.second, std::move(expr));
  }
  opoint_apply_speedz_default(opoint, field_or(frame, u"state"));
  return out;
}

}
