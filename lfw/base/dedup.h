#pragma once

#include <string>

namespace lfw {

template <typename F>
auto deduped(const std::u16string& , F&& body) {
  return body();
}

}
