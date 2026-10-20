#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/helper/randoming.h"

namespace lfw {

class Entity;
class MersenneTwister;

namespace controller {
class BaseController;
}

class LFW;

namespace helper {

class ObjectsHelper {
 public:
  static constexpr const char* TAG = "ObjectsHelper";

  explicit ObjectsHelper(LFW& lfw);
  virtual ~ObjectsHelper() = default;

  LFW& lfw() const { return *_lfw; }
  RandomingT<Value>& team_randoming() { return _team_randoming; }

  virtual std::vector<Entity*> all() const;
  Entity* a() const { return at(0); }
  Entity* b() const { return at(1); }
  Entity* at(double idx) const;

  virtual std::vector<Entity*> add(const Value& data, double num = 1,
                                   const std::u16string* team = nullptr);
  void del_all();

 protected:
  LFW* _lfw;
  RandomingT<Value> _team_randoming;
};

}
}
