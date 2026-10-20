#pragma once

#include "lfw/core/value.h"
#include "lfw/director/director.h"
#include "lfw/ui/component/tests/test_case.h"

namespace lfw {
namespace ui {

class DirectorTestCase : public TestCase {
 public:
  DirectorTestCase(Tests& owner, Value data);
  ~DirectorTestCase() override;

  std::optional<Value> update(double dt) override;
  void enter() override;
  void leave() override;

  const Value& data() const { return _data; }
  director::Director& playback() { return _playback; }

 private:
  Value _data;
  director::Director _playback;
};

}
}
