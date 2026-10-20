#pragma once

#include "lfw/controller/base_controller.h"

namespace lfw {
namespace controller {

class LocalController : public BaseController {
 public:
  static constexpr const char* TAG = "LocalController";

  LocalController() { set_kind(true, false); }
};

}
}
