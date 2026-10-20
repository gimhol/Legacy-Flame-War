#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

class Cases {
 public:
  explicit Cases(std::u16string name) : _name(std::move(name)) {}

  const std::u16string& name() const { return _name; }
  const std::u16string& separator() const { return _separator; }
  const std::vector<std::u16string>& cases() const { return _cases; }

  void reset();
  void push(const std::u16string& mark, const std::vector<Value>& args);
  std::u16string submit();

 private:
  std::u16string _name;
  std::u16string _separator = u"\uffe5";
  std::vector<std::u16string> _cases;
  uint64_t _times = 0;
};

Cases& mt_cases();
Cases& sus_cases();

}
