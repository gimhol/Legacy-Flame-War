#pragma once

#include "lfw/helper/entities_helper.h"

namespace lfw {
namespace helper {

class BallsHelper : public ObjectsHelper {
 public:
  using ObjectsHelper::ObjectsHelper;
  std::vector<Entity*> all() const override;
};

}
}
