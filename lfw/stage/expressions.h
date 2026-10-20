#pragma once

#include <memory>
#include <vector>

#include "lfw/utils/math/base.h"

namespace lfw {
namespace stage {

template <typename T>
class IExpression {
 public:
  virtual ~IExpression() = default;
  virtual bool run(const T& arg) = 0;
};

template <typename T>
class Expressions {
 public:
  using Items = std::vector<std::shared_ptr<IExpression<T>>>;

  const Items& list() const { return _items; }
  bool is_first() const { return _index <= 0.0; }
  bool is_last() const { return _index >= static_cast<double>(_items.size()) - 1.0; }
  double index() const { return _index; }

  void reset(const Items& list) {
    _index = 0.0;
    if (&list == &_items) return;
    _items.clear();
    if (!list.empty()) _items.insert(_items.end(), list.begin(), list.end());
  }

  bool run(const T& arg) {
    const double i = _index;
    if (i < 0.0 || i >= static_cast<double>(_items.size())) return false;
    return _items[static_cast<size_t>(i)]->run(arg);
  }

  void next() { _index = min(_index + 1.0, static_cast<double>(_items.size()) - 1.0); }

  bool flow(const T& arg) {
    bool pass = false;
    do {
      const bool is_last = this->is_last();
      pass = this->run(arg);
      if (!pass || is_last) break;
      this->next();
    } while (true);
    return pass;
  }

 private:
  Items _items;
  double _index = 0.0;
};

}
}
