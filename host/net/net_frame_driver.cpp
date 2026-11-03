#include "host/net/net_frame_driver.h"

#include <algorithm>
#include <utility>

namespace lfw::host::net {

void IFrameDriver::start(int first_seq) {
  _seq = first_seq;
  _dropped = 0;
  _pending.clear();
}

void IFrameDriver::push(FrameInputs inputs) {
  if (inputs.seq < _seq) {
    ++_dropped;
    return;
  }
  _pending[inputs.seq] = std::move(inputs);
}

bool IFrameDriver::can_step() const { return _pending.find(_seq) != _pending.end(); }

std::optional<FrameInputs> IFrameDriver::take() {
  const auto it = _pending.find(_seq);
  if (it == _pending.end()) return std::nullopt;
  FrameInputs out = std::move(it->second);
  _pending.erase(it);
  ++_seq;
  return out;
}

DelayDriver::DelayDriver(int lead) : _lead(std::max(1, lead)) {}

}
