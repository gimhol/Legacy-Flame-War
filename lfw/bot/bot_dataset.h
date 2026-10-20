#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {
namespace bot {

class BotDataSet {
 public:
  BotDataSet();

  static const BotDataSet& Default();

  const Value* get(const std::u16string& key) const;
  double num(const std::u16string& key) const;
  void assign(const Value& src);
  void reset();
  Value dump() const;

 private:
  Value data_;
};

}
}
