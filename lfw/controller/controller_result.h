#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"

namespace lfw {
namespace controller {

class ControllerResult {
 public:
  using Resolver = std::function<std::optional<ResolvedNextFrame>(const Value&)>;

  void set_resolver(Resolver fn) { _resolve = std::move(fn); }

  const Value& result() const { return _result; }
  /** 解析好的下一帧（`fire` 成功时一定有）。 */
  const ResolvedNextFrame* resolved() const { return _resolved.has_value() ? &*_resolved : nullptr; }
  const std::u16string& kind() const { return _kind; }
  const std::u16string& keys() const { return _keys; }
  double time() const { return _time; }

  ControllerResult& clear();
  bool fire(const Value& nf, double time, const std::u16string& keys,
            const std::u16string& kind);

 private:
  Resolver _resolve;
  std::optional<ResolvedNextFrame> _resolved;
  Value _result;
  std::u16string _kind;
  std::u16string _keys;
  double _time = 0;
};

}
}
