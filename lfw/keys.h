#pragma once

#include <string>
#include <utility>
#include <vector>

#include "lfw/controller/key_status.h"
#include "lfw/core/value.h"

namespace lfw {

class LFW;

class Keys {
 public:
  static constexpr const char* TAG = "Keys";

  explicit Keys(LFW& lfw);

  LFW& lfw() const;
  double time() const;

  controller::KeyStatus* get(const std::u16string& key);
  const std::vector<std::pair<std::u16string, controller::KeyStatus>>& list() const {
    return _keys;
  }

  void mount();
  void unmount();

 private:
  LFW* _lfw;
  std::vector<std::pair<std::u16string, controller::KeyStatus>> _keys;
};

}
