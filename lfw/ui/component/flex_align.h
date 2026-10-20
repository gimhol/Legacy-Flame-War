#pragma once

#include <string>
#include <vector>

namespace lfw {
namespace ui {

enum class FlexAlign { Start, Center, End, Stretch };

inline const std::vector<std::u16string>& ALL_FLEX_ALIGN() {
  static const std::vector<std::u16string> v{u"start", u"center", u"end", u"stretch"};
  return v;
}

}
}
