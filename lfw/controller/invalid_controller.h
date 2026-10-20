#pragma once

#include "lfw/controller/base_controller.h"

namespace lfw {
namespace controller {

class InvalidController : public BaseController {
 public:
  static constexpr const char* TAG = "InvalidController";

  bool is_invalid_controller() const override { return true; }
};

}
}
