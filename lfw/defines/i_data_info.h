#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

struct IDataInfo {
  Value type;
  Value url;
  Value title;
  Value description;
  Value author;
  Value version;
  Value time;
  Value md5;
};

}
