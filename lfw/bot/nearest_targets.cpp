#include "lfw/bot/nearest_targets.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/entity/entity.h"
#include "lfw/helper/manhattan_xz.h"

namespace lfw {
namespace bot {


bool target_less(const std::pair<double, const Entity*>& a,
                 const std::pair<double, const Entity*>& b) {
  const double d = a.first - b.first;
  if (d != 0.0) {
    if (std::isnan(d)) return false;
    return d < 0.0;
  }
  const Value ia = Value(a.second->id);
  const Value ib = Value(b.second->id);
  if (lt(ia, ib)) return true;
  if (gt(ia, ib)) return false;
  return false;
}

const Entity* NearestTargets::get() const { return targets_.empty() ? nullptr : targets_[0]; }

bool NearestTargets::has_target(const Entity* v) const {
  for (const Entity* t : targets_) {
    if (t == v) return true;
  }
  return false;
}

void NearestTargets::look(const Entity& self, const Entity& other) {
  look_impl([&self](const Entity* t) { return helper::manhattan_xz(self, *t); }, &other);
}

void NearestTargets::del(const std::function<bool(const Entity&)>& condition) {
  std::vector<const Entity*> kept;
  kept.reserve(targets_.size());
  for (const Entity* t : targets_) {
    if (!condition(*t)) kept.push_back(t);
  }
  targets_ = std::move(kept);
}

void NearestTargets::sort(const Entity& self) {
  sort_by([&self](const Entity* t) { return helper::manhattan_xz(self, *t); });
}

void NearestTargets::clear() { targets_.clear(); }

}
}