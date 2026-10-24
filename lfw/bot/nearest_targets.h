#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace lfw {

class Entity;

namespace bot {

bool target_less(const std::pair<double, const Entity*>& a,
                 const std::pair<double, const Entity*>& b);

class NearestTargets {
 public:
  explicit NearestTargets(double max) : max_(max) {}

  const Entity* get() const;
  void look(const Entity& self, const Entity& other);
  void del(const std::function<bool(const Entity&)>& condition);
  /** `del(&t == &other)` 的免 `std::function` 版本：目标搜索里每个被访问的实体都会走一次。 */
  void del_ptr(const Entity* target);
  void sort(const Entity& self);
  void clear();

  double max() const { return max_; }
  const std::vector<const Entity*>& targets() const { return targets_; }
  std::size_t size() const { return targets_.size(); }
  bool empty() const { return targets_.empty(); }

 private:
  bool has_target(const Entity* v) const;

  template <typename DistanceOf>
  void look_impl(const DistanceOf& distance_of, const Entity* other) {
    if (other == nullptr) return;
    if (has_target(other)) return;
    const double distance = distance_of(other);
    const std::size_t len = targets_.size();
    if (static_cast<double>(len) < max_) {
      targets_.push_back(other);
      return;
    }
    for (std::size_t i = 0; i < len; ++i) {
      if (distance > distance_of(targets_[i])) continue;
      targets_.insert(targets_.begin() + static_cast<std::ptrdiff_t>(i), other);
      targets_.erase(targets_.begin() + static_cast<std::ptrdiff_t>(max_), targets_.end());
      break;
    }
  }

  template <typename DistanceOf>
  void sort_by(const DistanceOf& distance_of) {
    std::vector<std::pair<double, const Entity*>> tmp;
    tmp.reserve(targets_.size());
    for (const Entity* t : targets_) tmp.emplace_back(distance_of(t), t);
    std::stable_sort(tmp.begin(), tmp.end(), target_less);
    for (std::size_t i = 0; i < tmp.size(); ++i) targets_[i] = tmp[i].second;
  }

  double max_;
  std::vector<const Entity*> targets_;
};

}
}