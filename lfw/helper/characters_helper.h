#pragma once

#include <functional>

#include "lfw/helper/entities_helper.h"

namespace lfw {
namespace helper {

class CharactersHelper : public ObjectsHelper {
 public:
  using ObjectsHelper::ObjectsHelper;
  std::vector<Entity*> all() const override;
  std::vector<Entity*> add(const Value& data, double num = 1,
                           const std::u16string* team = nullptr) override;
  std::vector<Entity*> add_random(double num = 1, const std::u16string* team = nullptr,
                                  const std::function<bool(const Value&)>* filter = nullptr);
};

}
}
