#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/helper/randoming.h"

namespace lfw {

class LFW;

class MersenneTwister;
class Resources;

namespace loader {


class DatMgr {
 public:
  static constexpr const char* kTag = "DatMgr";

  using FindPredicate =
      std::function<bool(const Value& value, double index, const std::vector<Value>& arr)>;

  explicit DatMgr(LFW* host);

  int inner_id() const { return _inner_id; }

  bool load(const std::vector<std::u16string>& index_files, std::u16string& error);
  void dispose();
  void clear();

  const std::vector<Value>& bots() const;
  const std::vector<Value>& moves() const;
  const std::vector<Value>& objects() const;
  const std::vector<Value>& fighters() const;
  const std::vector<Value>& weapons() const;
  const std::vector<Value>& balls() const;
  const std::vector<Value>& entities() const;
  const std::vector<Value>& backgrounds() const;
  const std::vector<Value>& stages() const;

  const std::vector<Value>& directors() const;
  const Value* find_director(const Value& id) const;

  const Value* find(const Value& id) const;
  const Value* find_bot(const Value& id) const;
  const Value* find_moves(const Value& id) const;

  Randoming::Ptr get_randoming_by_group(const std::u16string& group);

  const Value* find_weapon(const Value& id) const;
  const Value* find_weapon(const FindPredicate& predicate) const;
  const Value* find_entity(const Value& id) const;
  const Value* find_entity(const FindPredicate& predicate) const;
  const Value* find_object(const Value& id) const;
  const Value* find_object(const FindPredicate& predicate) const;
  const Value* find_fighter(const Value& id) const;
  const Value* find_fighter(const FindPredicate& predicate) const;
  const Value* find_background(const Value& id) const;
  const Value* find_background(const FindPredicate& predicate) const;

  std::vector<Value> get_objects_of_group(const std::u16string& group) const;
  std::vector<Value> get_fighters_of_group(const std::u16string& group) const;
  std::vector<Value> get_weapons_of_group(const std::u16string& group) const;
  std::vector<Value> get_fighters_not_in_group(const std::u16string& group) const;
  std::vector<Value> get_backgrouds_of_group(const std::u16string& group) const;

  Randoming::Ptr get_bg_randoming_of_group(const std::vector<std::u16string>& groups);
  Value get_random_bg(const std::vector<std::u16string>& groups);

 private:
  struct Inner;

  LFW* _host = nullptr;
  int _inner_id = 0;
  std::shared_ptr<Inner> _inner;
};

}
}
