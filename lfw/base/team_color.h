#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/team_enum.h"

namespace lfw {

std::u16string get_team_text_color(const std::u16string& team, const std::u16string* fallback = nullptr);
std::u16string get_team_outline_color(const std::u16string& team);

namespace team_color_detail {

inline const Object* team_info(const std::u16string& team) {
  const Value* map = defines::find(u"Defines.TeamInfoMap");
  const Object* obj = map != nullptr ? as_object(*map) : nullptr;
  if (obj == nullptr) return nullptr;
  const Value* info = obj->get(team);
  return info != nullptr ? as_object(*info) : nullptr;
}

inline const std::u16string* team_field(const Object* info, const char16_t* key) {
  if (info == nullptr) return nullptr;
  const Value* v = info->get(std::u16string(key));
  return v != nullptr ? std::get_if<std::u16string>(v) : nullptr;
}

}

inline std::u16string get_team_text_color(const std::u16string& team, const std::u16string* fallback) {
  const std::u16string* color = team_color_detail::team_field(
      team_color_detail::team_info(team), u"txt_color");
  if (color != nullptr && !color->empty()) return *color;
  if (fallback != nullptr) return *fallback;
  const std::u16string* ind = team_color_detail::team_field(
      team_color_detail::team_info(std::u16string(team_enum::kIndependent)), u"txt_color");
  return ind != nullptr ? *ind : std::u16string();
}

inline std::u16string get_team_outline_color(const std::u16string& team) {
  const Object* info = team_color_detail::team_info(team);
  if (info == nullptr) info = team_color_detail::team_info(std::u16string(team_enum::kIndependent));
  const std::u16string* color = team_color_detail::team_field(info, u"txt_outline_color");
  return color != nullptr ? *color : std::u16string();
}

}
