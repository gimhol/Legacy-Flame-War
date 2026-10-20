#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/utils/math/mersenne_twister.h"

namespace lfw {

MersenneTwister& randoming_default_mt();

template <typename T>
struct RandomingItem;

template <>
struct RandomingItem<Value> {
  static Value null_taken() { return Value(NullTag{}); }
  static Value out_of_range() { return Value(); }
  static bool loose_ne(const Value& a, const Value& b) { return !equals(a, b); }
};

template <typename T>
struct RandomingItem<std::shared_ptr<T>> {
  static std::shared_ptr<T> null_taken() { return nullptr; }
  static std::shared_ptr<T> out_of_range() { return nullptr; }
  static bool loose_ne(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) { return a != b; }
};

template <typename T>
class RandomingT {
 public:
  using Ptr = std::shared_ptr<RandomingT<T>>;

  static MersenneTwister& default_mt();

  RandomingT(std::u16string name, std::vector<T> src, MersenneTwister* mt = nullptr,
             Value duplicate = Value(false));

  static Ptr create(std::u16string name, std::vector<T> src, MersenneTwister* mt = nullptr,
                    Value duplicate = Value(false));

  RandomingT& set_src(std::vector<T> src);

  T get();

  const std::u16string& name() const { return _name; }
  MersenneTwister* mt() const { return _mt; }
  const std::vector<T>& src() const { return _src; }
  const std::vector<T>& cur() const { return _cur; }
  const T& taken() const { return _taken; }
  const Value& duplicate() const { return _duplicate; }

 private:
  T random_get();
  T random_take();
  double random_in(double l, double r);

  std::u16string _name;
  MersenneTwister* _mt = nullptr;
  std::vector<T> _src;
  std::vector<T> _cur;
  T _taken = RandomingItem<T>::null_taken();
  Value _duplicate = Value(false);
};

using Randoming = RandomingT<Value>;

}
