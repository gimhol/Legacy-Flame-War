#pragma once

#include <cstdint>

namespace lfw {

enum class EnterFrameResult : int {
  Gone = 0,
  NotFound = 1,
  Entered = 2,
  Fallback = 3,
};

inline const char16_t* enter_frame_result_name(EnterFrameResult r) {
  switch (r) {
    case EnterFrameResult::Gone:
      return u"Gone";
    case EnterFrameResult::NotFound:
      return u"NotFound";
    case EnterFrameResult::Entered:
      return u"Entered";
    case EnterFrameResult::Fallback:
      return u"Fallback";
  }
  return u"";
}

}
