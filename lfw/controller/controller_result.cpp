#include "lfw/controller/controller_result.h"

#include <variant>

#include "lfw/utils/type_cast.h"

namespace lfw {
namespace controller {

ControllerResult& ControllerResult::clear() {
  _result = Value();
  _resolved.reset();
  _time = 0;
  _keys.clear();
  _kind.clear();
  return *this;
}

bool ControllerResult::fire(const Value& nf, double time,
                            const std::u16string& keys,
                            const std::u16string& kind) {
  const std::optional<ResolvedNextFrame> resolved =
      _resolve ? _resolve(nf) : std::nullopt;
  if (!resolved.has_value()) return false;
  _resolved = *resolved;
  // JS / bot 那边还按 `{frame, which}` 形状看结果，这里同步一份
  Object mirror;
  mirror.set(u"frame", resolved->frame != nullptr ? resolved->frame->to_value() : Value());
  mirror.set(u"which", resolved->which);
  _result = Value(std::make_shared<Object>(mirror));
  _time = time;
  _keys = keys;
  _kind = kind;
  return true;
}

}
}
