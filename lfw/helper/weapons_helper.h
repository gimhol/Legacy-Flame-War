#pragma once

#include <string>
#include <utility>
#include <vector>

#include "lfw/helper/entities_helper.h"

namespace lfw {
namespace helper {

class WeaponsHelper : public ObjectsHelper {
 public:
  using ObjectsHelper::ObjectsHelper;
  std::vector<Entity*> all() const override;
  std::vector<Entity*> add(const Value& data, double num = 1,
                           const std::u16string* team = nullptr) override;
  RandomingT<Value>* randoms(const std::u16string& groups, bool duplicate);
  std::vector<Entity*> add_random(double num = 1, bool duplicate = false,
                                  const std::u16string& group = std::u16string());

 private:
  std::vector<std::pair<std::u16string, RandomingT<Value>::Ptr>> _random_map;
  std::vector<std::pair<std::u16string, RandomingT<Value>::Ptr>> _random_d_map;
};

}
}
