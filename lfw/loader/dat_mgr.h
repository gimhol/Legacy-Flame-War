#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/bot/bot_data.h"
#include "lfw/data/background_data.h"
#include "lfw/data/entity_data.h"
#include "lfw/data/stage_data.h"
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

  // 加载期就已经解析好的具体类型。`resolve_*` 命中缓存时返回缓存那一份
  //（共享同一个 Object，语义与直接传 Value 一致）；运行期自造的数据对象就地构造一份。
  const bot::BotData* find_bot_data(const Value& id) const;
  const EntityData* find_entity_data(const Value& id) const;
  const StageData* find_stage_data(const Value& id) const;
  const BackgroundData* find_bg_data(const Value& id) const;
  bot::BotData resolve_bot_data(const Value& data) const;
  /**
   * 解析成 `EntityData`。返回的指针**永不失效**：命中的是加载期缓存那一份，
   * 没命中的就地解析一份留在 `Inner` 里（运行期没人释放实体数据）。
   */
  const EntityData* resolve_entity_data(const Value& data) const;
  StageData resolve_stage_data(const Value& data) const;
  BackgroundData resolve_bg_data(const Value& data) const;
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
