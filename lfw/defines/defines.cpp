#include "lfw/defines/defines_data.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "lfw/core/json5.h"
#include "lfw/defines/cheat_type.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/runtime_gen.h"
#include "lfw/utils/math/base.h"

namespace lfw {
namespace defines {
namespace {

std::vector<std::pair<std::u16string, Value>> parse_all() {
  const std::vector<DefinesRuntimeEntry>& src = defines_runtime_entries();
  std::vector<std::pair<std::u16string, Value>> out;
  out.reserve(src.size());
  for (const DefinesRuntimeEntry& e : src) {
    Json5Result r = json5_parse(e.value_json5);
    out.emplace_back(e.name, std::move(r.value));
  }
  return out;
}

}  // namespace

const std::vector<std::pair<std::u16string, Value>>& table() {
  static const std::vector<std::pair<std::u16string, Value>> kTable = parse_all();
  return kTable;
}

namespace {

// `find()` 在热路径上（bot 每次目标搜索、实体物理常量）会被调几十万次，原来是线性扫 66 条
// 再逐条比字符串。表初始化后不再变，所以建一张 `string_view → Value*`（指向表内的键与值）。
const std::unordered_map<std::u16string_view, const Value*>& table_index() {
  static const std::unordered_map<std::u16string_view, const Value*> kIndex = [] {
    std::unordered_map<std::u16string_view, const Value*> m;
    m.reserve(table().size() * 2);
    for (const std::pair<std::u16string, Value>& kv : table()) {
      m.emplace(std::u16string_view(kv.first), &kv.second);
    }
    return m;
  }();
  return kIndex;
}

}  // namespace

const Value* find(std::u16string_view name) {
  const auto& index = table_index();
  const auto it = index.find(name);
  return it != index.end() ? it->second : nullptr;
}

double num(const char16_t* name) {
  const Value* v = find(std::u16string_view(name));
  if (v == nullptr) return 0;
  if (const double* d = std::get_if<double>(v)) return *d;
  return to_number(*v);
}

double desire(double ratio) { return round(num(u"Defines.MAX_AI_DESIRE") * ratio); }

const Object* get_default_keys(const std::u16string& player_id) {
  const Value* m = find(u"Defines.default_keys_map");
  const Object* o = m != nullptr ? as_object(*m) : nullptr;
  if (o == nullptr) return nullptr;
  const Value* exact = o->get(player_id);
  if (exact != nullptr) {
    const Object* r = as_object(*exact);
    if (r != nullptr) return r;
  }
  const Value* fallback = o->get(u"_");
  return fallback != nullptr ? as_object(*fallback) : nullptr;
}

Value get_default_keys_value(const std::u16string& player_id) {
  const Value* m = find(u"Defines.default_keys_map");
  const Object* o = m != nullptr ? as_object(*m) : nullptr;
  if (o == nullptr) return Value();
  const Value* exact = o->get(player_id);
  // TS 是 `map.get(id) || map.get('_')!` ⇒ 假值也算没取到。
  if (exact != nullptr && truthy(*exact)) return *exact;
  const Value* fallback = o->get(u"_");
  return fallback != nullptr ? *fallback : Value();
}

bool is_independent(const std::u16string& team) { return team.size() != 1; }

bool is_cheat_type(const std::u16string& v) {
  return v == std::u16string(cheat_enum::kLF2_NET) || v == std::u16string(cheat_enum::kHERO_FT) ||
         v == std::u16string(cheat_enum::kGIM_INK);
}

bool is_difficulty(double v) {
  return v == static_cast<double>(Difficulty::Easy) || v == static_cast<double>(Difficulty::Normal) ||
         v == static_cast<double>(Difficulty::Difficult) || v == static_cast<double>(Difficulty::Crazy);
}

}

}
