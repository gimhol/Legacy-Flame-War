#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IHitKeyMap`（`src/LFW/defines/IHitKeyMap.ts`）。
 *
 * 16 个固定的按键 → 下一帧映射；每个值仍是 `TNextFrame`（`object | string | array`），
 * 交给帧导航消费，所以这里保持 `Value`，但键名固定下来，不再是满地
 * `obj_get(hit, u"F")` / `field_or(hold, u"B")`。
 */
class HitKeyMap {
 public:
  HitKeyMap() = default;
  explicit HitKeyMap(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const Value& a() const { return _a; }
  const Value& j() const { return _j; }
  const Value& d() const { return _d; }
  const Value& U() const { return _U; }
  const Value& D() const { return _D; }
  const Value& L() const { return _L; }
  const Value& R() const { return _R; }
  const Value& F() const { return _F; }
  const Value& B() const { return _B; }
  const Value& aa() const { return _aa; }
  const Value& jj() const { return _jj; }
  const Value& dd() const { return _dd; }
  const Value& FF() const { return _FF; }
  const Value& BB() const { return _BB; }
  const Value& UU() const { return _UU; }
  const Value& DD() const { return _DD; }

  /** 键名是运行期算出来的场合（例如 `check_key_act`）。 */
  const Value* find(const std::u16string& key) const;

  /** 表达式引擎 / JS 桥专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  Value _a;
  Value _j;
  Value _d;
  Value _U;
  Value _D;
  Value _L;
  Value _R;
  Value _F;
  Value _B;
  Value _aa;
  Value _jj;
  Value _dd;
  Value _FF;
  Value _BB;
  Value _UU;
  Value _DD;
};

}