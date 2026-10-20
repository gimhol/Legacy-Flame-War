#pragma once

#include <string>

#include "lfw/core/js_string.h"

namespace lfw {

inline std::u16string get_short_file_size_txt(double bytes) {
  const auto strip_dot_zero = [](const std::u16string& s) {
    const size_t at = s.find(u".0");
    if (at == std::u16string::npos) return s;
    return s.substr(0, at) + s.substr(at + 2);
  };
  if (bytes < 1024) return number_to_string(bytes) + u"B";
  bytes /= 1024;
  if (bytes < 1024) return strip_dot_zero(number_to_fixed_1(bytes)) + u"KB";
  bytes /= 1024;
  if (bytes < 1024) return strip_dot_zero(number_to_fixed_1(bytes)) + u"MB";
  bytes /= 1024;
  return strip_dot_zero(number_to_fixed_1(bytes)) + u"GB";
}

}
