#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
class LFW;

namespace helper {

class UIHelper {
 public:
  static constexpr const char* TAG = "UIHelper";

  explicit UIHelper(LFW& lfw) : _lfw(&lfw) {}

  const std::vector<Value>& all() const { return _all; }

  UIHelper& clear();
  UIHelper& add(const std::vector<Value>& uis);

  void push_page(const std::u16string& id, double stack_idx = 0);
  void set_page(const std::u16string& id, double stack_idx = 0);

 private:
  LFW* _lfw;
  std::vector<Value> _all;
};

}
}
