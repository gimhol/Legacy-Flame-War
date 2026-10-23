#include "lfw/loader/dat_mgr.h"

#include "lfw/lfw.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "lfw/controller/creators.h"
#include "lfw/core/js_string.h"
#include "lfw/core/same_ref.h"
#include "lfw/core/value.h"
#include "lfw/dat_translator/value_builder.h"
#include "lfw/dat_translator/xml/xml_to_data_lists.h"
#include "lfw/dat_translator/xml/xml_x_bg_data.h"
#include "lfw/dat_translator/xml/xml_x_director.h"
#include "lfw/dat_translator/xml/xml_x_entity_data.h"
#include "lfw/dat_translator/xml/xml_x_stage_info.h"
#include "lfw/defines/defines.h"
#include "lfw/defines/defines_data.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/factory.h"
#include "lfw/loader/check_stage_info.h"
#include "lfw/loader/normalize_directors.h"
#include "lfw/loader/preprocess_bg_data.h"
#include "lfw/loader/preprocess_bot_data.h"
#include "lfw/loader/preprocess_entity_data.h"
#include "lfw/loader/preprocess_stage.h"
#include "lfw/resources.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/mersenne_twister.h"
#include "lfw/utils/type_check.h"

namespace lfw {
namespace loader {

namespace {

constexpr size_t kNpos = static_cast<size_t>(-1);

Object* as_mut(const Value& v) { return const_cast<Object*>(as_object(v)); }

bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool ends_with(const std::u16string& s, const std::u16string& tail) {
  if (s.size() < tail.size()) return false;
  return s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

size_t index_of_key(const std::vector<std::pair<Value, Value>>& list, const Value& key) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (strict_equals(list[i].first, key)) return i;
  }
  return kNpos;
}

void map_set(std::vector<std::pair<Value, Value>>& list, const Value& key, const Value& value) {
  const size_t i = index_of_key(list, key);
  if (i == kNpos) {
    list.push_back(std::make_pair(key, value));
    return;
  }
  list[i].second = value;
}

Value map_get(const std::vector<std::pair<Value, Value>>& list, const Value& key) {
  const size_t i = index_of_key(list, key);
  return i == kNpos ? Value() : list[i].second;
}

const Value* map_get_ptr(const std::vector<std::pair<Value, Value>>& list, const Value& key) {
  const size_t i = index_of_key(list, key);
  return i == kNpos ? nullptr : &list[i].second;
}

bool same_set_key(const Value& a, const Value& b) {
  const Object* const oa = as_object(a);
  const Object* const ob = as_object(b);
  if (oa != nullptr || ob != nullptr) return oa == ob;
  return strict_equals(a, b);
}

std::u16string join_strings(const std::vector<std::u16string>& items, const std::u16string& sep) {
  std::u16string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i != 0) out += sep;
    out += items[i];
  }
  return out;
}

Value strings_to_array(const std::vector<std::u16string>& items) {
  std::vector<Value> out;
  out.reserve(items.size());
  for (const std::u16string& s : items) out.push_back(Value(s));
  return Value(std::make_shared<Array>(std::move(out)));
}

Value make_arr_of(const std::vector<Value>& items) {
  return Value(std::make_shared<Array>(items));
}

size_t find_index_by_id(const std::vector<Value>& list, const Value& id) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (strict_equals(field_or(list[i], u"id"), id)) return i;
  }
  return kNpos;
}

const Value* find_by_id(const std::vector<Value>& list, const Value& id) {
  const size_t i = find_index_by_id(list, id);
  return i == kNpos ? nullptr : &list[i];
}

const Value* find_by_predicate(const std::vector<Value>& list, const DatMgr::FindPredicate& pred) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (pred(list[i], static_cast<double>(i), list)) return &list[i];
  }
  return nullptr;
}

long js_index_of(const Value& holder, const Value& item) {
  if (const Array* const a = as_array(holder)) {
    for (size_t i = 0; i < a->size(); ++i) {
      if (strict_equals(a->at(i), item)) return static_cast<long>(i);
    }
    return -1;
  }
  if (std::holds_alternative<std::u16string>(holder)) {
    const std::u16string hay = std::get<std::u16string>(holder);
    const std::u16string needle = to_string(item);
    const size_t at = hay.find(needle);
    return at == std::u16string::npos ? -1 : static_cast<long>(at);
  }
  return -1;
}


}

struct DatMgr::Inner {
  DatMgr* mgr = nullptr;
  int id = 0;
  LFW* host = nullptr;

  std::vector<std::pair<std::u16string, std::vector<Value>>> datas;
  std::vector<std::pair<Value, Value>> data_map;
  // 加载期解析出来的具体类型（按 id 索引），运行期只读。
  std::map<std::u16string, bot::BotData> bot_datas;
  std::map<std::u16string, EntityData> entity_datas;
  // esolve_entity_data 兜底现解析的那些（数据表里没有的）：解析一次就留着，指针永不失效。
  std::vector<std::unique_ptr<EntityData>> ad_hoc_entity_datas;
  std::vector<std::unique_ptr<bot::BotData>> ad_hoc_bot_datas;
  std::vector<std::unique_ptr<StageData>> ad_hoc_stage_datas;
  std::vector<std::unique_ptr<BackgroundData>> ad_hoc_bg_datas;
  std::map<std::u16string, StageData> stage_datas;
  std::map<std::u16string, BackgroundData> bg_datas;
  std::vector<std::pair<Value, Value>> alias_map;
  std::vector<Value> stages;
  std::vector<std::pair<Value, Value>> bot_map;
  std::vector<std::pair<Value, Value>> moves_map;
  std::vector<std::pair<std::u16string, Randoming::Ptr>> randomings;
  std::vector<std::pair<std::u16string, Randoming::Ptr>> bg_randomings;

  Inner(DatMgr* mgr_in, int id_in) : mgr(mgr_in), id(id_in), host(mgr_in->_host) {
    datas.push_back({u"4", {}});
    datas.push_back({u"8", {}});
    datas.push_back({u"16", {}});
    datas.push_back({u"32", {}});
    std::vector<Value> bg;
    if (const Value* const void_bg = defines::find(u"Defines.VOID_BG")) bg.push_back(*void_bg);
    datas.push_back({u"background", std::move(bg)});
    datas.push_back({u"objects", {}});
    datas.push_back({u"bots", {}});
    datas.push_back({u"moves", {}});
    datas.push_back({u"directors", {}});
    if (const Value* const void_stage = defines::find(u"Defines.VOID_STAGE")) {
      stages.push_back(*void_stage);
    }
  }

  bool cancelled() const { return mgr->inner_id() != id; }
  bool check_cancelled(std::u16string& error) {
    if (!cancelled()) return true;
    error = u"cancelled";
    return false;
  }

  std::vector<Value>* find_list(const std::u16string& key) {
    for (std::pair<std::u16string, std::vector<Value>>& kv : datas) {
      if (kv.first == key) return &kv.second;
    }
    return nullptr;
  }

  std::vector<Value>* find_list_of_type(const Value& type) {
    if (std::holds_alternative<std::u16string>(type)) {
      return find_list(std::get<std::u16string>(type));
    }
    if (std::holds_alternative<double>(type)) {
      return find_list(number_to_string(std::get<double>(type)));
    }
    return nullptr;
  }

  std::vector<Value>& list_ref(const char16_t* key) { return *find_list(std::u16string(key)); }

  bool cook_data(const Value& raw, Value& out, std::u16string& error) {
    if (entity::is_bg_data(raw)) {
      std::vector<std::u16string> warnings;
      std::vector<std::u16string> errors;
      Value data = raw;
      out = preprocess_bg_data(data, &warnings, &errors);
      if (!warnings.empty()) host->warn({strings_to_array(warnings)});
      if (!errors.empty()) host->error({strings_to_array(errors)});
      return true;
    }
    const Value id = field_or(raw, u"id");
    if (entity::is_ball_data(raw)) {
      Factory::register_ctrl(id, controller::ball_controller_creator());
    } else if (entity::is_weapon_data(raw)) {
      Factory::register_ctrl(id, controller::ball_controller_creator());
    } else if (entity::is_fighter_data(raw)) {
      Factory::register_ctrl(id, controller::bot_controller_creator());
    }
    Value base = field_or(raw, u"base");
    Object* const base_o = as_mut(base);
    if (base_o == nullptr) {
      error = is_nullish(raw) ? u"Cannot read properties of undefined (reading 'base')"
                              : u"Cannot read properties of undefined (reading 'bot')";
      return false;
    }
    const Value bot_prev = field_or(base, u"bot");
    const Value bot = is_nullish(bot_prev)
                          ? map_get(bot_map, is_nullish(id) ? field_or(base, u"bot_id") : id)
                          : bot_prev;
    base_o->set(u"bot", bot);

    Value ctx = dat_translator::make_obj({
        {u"lfw", dat_translator::make_obj({{u"images", dat_translator::make_obj({})},
                                           {u"sounds", dat_translator::make_obj({})}})},
        {u"data", raw},
        {u"jobs", dat_translator::make_arr({})},
        {u"errors", dat_translator::make_arr({})},
    });
    if (!preprocess_entity_data(ctx, error)) return false;
    out = field_or(ctx, u"data");
    return true;
  }

  void add_alias_object(const Value& alias, const Value& data) {
    const Value prev = map_get(alias_map, alias);
    if (truthy(prev)) {
      host->warn({Value(u"DatMgr::_add_obj"),
                  Value(u"alias duplicated, old data will be overwritten!"), Value(u"old data:"),
                  prev, Value(u"new data:"), data});
    }
    map_set(alias_map, alias, data);
  }

  bool add_object(const Value& key, const Value& data, std::u16string& error) {
    const Value prev = map_get(data_map, key);
    if (truthy(prev)) {
      host->warn({Value(u"DatMgr::_add_obj"),
                  Value(u"id duplicated, old data will be overwritten!"), Value(u"old data:"),
                  prev, Value(u"new data:"), data});
    }
    map_set(data_map, key, data);
    entity_datas[to_string(key)] = EntityData(data);

    const Value data_id = field_or(data, u"id");
    std::vector<Value>* const list = find_list_of_type(field_or(data, u"type"));
    if (list == nullptr) {
      error = u"Cannot read properties of undefined (reading 'findIndex')";
      return false;
    }
    {
      const size_t idx = find_index_by_id(*list, data_id);
      if (idx == kNpos) {
        list->push_back(data);
      } else {
        (*list)[idx] = data;
      }
    }
    {
      std::vector<Value>& objects = list_ref(u"objects");
      const size_t idx = find_index_by_id(objects, data_id);
      if (idx == kNpos) {
        objects.push_back(data);
      } else {
        objects[idx] = data;
      }
    }
    return true;
  }

  bool add_bg(const Value& data, std::u16string& error) {
    bg_datas[to_string(field_or(data, u"id"))] = BackgroundData(data);
    std::vector<Value>* const list = find_list_of_type(field_or(data, u"type"));
    if (list == nullptr) {
      error = u"Cannot read properties of undefined (reading 'findIndex')";
      return false;
    }
    const Value data_id = field_or(data, u"id");
    const size_t idx = find_index_by_id(*list, data_id);
    if (idx == kNpos) {
      list->push_back(data);
    } else {
      (*list)[idx] = data;
    }
    const Value group = field_or(field_or(data, u"base"), u"group");
    if (const Array* const a = as_array(group)) {
      for (size_t i = 0; i < a->size(); ++i) {
        if (!std::holds_alternative<std::u16string>(a->at(i))) continue;
        const std::u16string g = std::get<std::u16string>(a->at(i));
        for (size_t j = 0; j < bg_randomings.size();) {
          if (bg_randomings[j].first == g) {
            bg_randomings.erase(bg_randomings.begin() + static_cast<std::ptrdiff_t>(j));
          } else {
            ++j;
          }
        }
      }
    }
    return true;
  }

  static bool append_items(const Value& partial, const char16_t* key, std::vector<Value>& out,
                           std::u16string& error) {
    const Value v = field_or(partial, key);
    if (is_nullish(v)) return true;
    const Array* const a = as_array(v);
    if (a == nullptr) {
      error = u"spread of non-iterable";
      return false;
    }
    for (size_t i = 0; i < a->size(); ++i) out.push_back(a->at(i));
    return true;
  }

  bool solve_index_files(const std::vector<std::u16string>& index_files, Value& out,
                         std::u16string& error) {
    if (!check_cancelled(error)) return false;
    std::vector<Value> objects;
    std::vector<Value> backgrounds;
    std::vector<Value> stages_out;
    std::vector<Value> bots_out;
    std::vector<Value> moves_out;
    std::vector<Value> directors_out;
    Resources& resources = host->resources();
    for (const std::u16string& file : index_files) {
      if (!check_cancelled(error)) return false;
      const std::u16string f = to_lower_case(file);
      Value partial;
      if (ends_with(f, u".xml")) {
        ImportResult result;
        if (!resources.import_xml(file, true, result, error)) return false;
        if (!check_cancelled(error)) return false;
        if (result.xml_root == nullptr) {
          error = u"missing xml root";
          return false;
        }
        partial = dat_translator::xml::xml_2_data_lists(*result.xml_root);
      } else if (ends_with(f, u".json") || ends_with(f, u".json5")) {
        ImportResult result;
        if (!resources.import_json(file, true, result, error)) return false;
        partial = result.data;
      } else {
        host->warn({Value(u"UNSUPPORTED DAT INDEX FILE, SKIPPED: " + file)});
        continue;
      }
      if (!check_cancelled(error)) return false;
      if (!append_items(partial, u"objects", objects, error)) return false;
      if (!append_items(partial, u"backgrounds", backgrounds, error)) return false;
      if (!append_items(partial, u"stages", stages_out, error)) return false;
      if (!append_items(partial, u"bots", bots_out, error)) return false;
      if (!append_items(partial, u"moves", moves_out, error)) return false;
      if (!append_items(partial, u"directors", directors_out, error)) return false;
    }
    out = dat_translator::make_obj({
        {u"objects", make_arr_of(objects)},
        {u"backgrounds", make_arr_of(backgrounds)},
        {u"stages", make_arr_of(stages_out)},
        {u"bots", make_arr_of(bots_out)},
        {u"moves", make_arr_of(moves_out)},
        {u"directors", make_arr_of(directors_out)},
    });
    return true;
  }

  bool load(const std::vector<std::u16string>& index_files, std::u16string& error) {
    Resources& resources = host->resources();
    for (const EnumTextEntry& e : built_in_imgs_entries()) {
      if (!is_non_blank_str(Value(std::u16string(e.text)))) continue;
      host->emit_progress(e.text, 0);
      host->load_img(e.text);
      if (!check_cancelled(error)) return false;
    }
    for (const EnumTextEntry& e : built_in_dats_entries()) {
      if (!is_non_blank_str(Value(std::u16string(e.text)))) continue;
      host->emit_progress(e.text, 0);
      ImportResult result;
      if (!resources.import_json(e.text, true, result, error)) return false;
      Value cooked;
      if (!cook_data(result.data, cooked, error)) return false;
      if (!add_object(Value(std::u16string(e.text)), cooked, error)) return false;
      if (!check_cancelled(error)) return false;
    }

    Value data;
    if (!solve_index_files(index_files, data, error)) return false;
    if (!check_cancelled(error)) return false;

    const Array* const bots_list = as_array(field_or(data, u"bots"));
    const Array* const moves_list = as_array(field_or(data, u"moves"));
    const Array* const objects_list = as_array(field_or(data, u"objects"));
    const Array* const bgs_list = as_array(field_or(data, u"backgrounds"));
    const Array* const stages_list = as_array(field_or(data, u"stages"));
    const Array* const directors_list = as_array(field_or(data, u"directors"));
    const double total = static_cast<double>(bots_list->size() + moves_list->size() +
                                             directors_list->size() + objects_list->size() +
                                             bgs_list->size() + stages_list->size());
    double loaded = 0;
    const auto report = [&](const Value& file) {
      host->emit_progress(to_string(file),
                          total != 0 ? std::floor((loaded * 100) / total) : 0);
    };

    for (size_t i = 0; i < bots_list->size(); ++i) {
      const Value item = bots_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      const std::u16string file = to_string(field_or(item, u"file"));
      report(Value(file));
      ImportResult result;
      Value raw;
      if (resources.import_json(file, true, result, error)) {
        raw = result.data;
      } else {
        host->warn({Value(u"FAILED TO LOAD BOT DATA: " + file)});
        error.clear();
      }
      if (!check_cancelled(error)) return false;
      if (!truthy(raw)) continue;
      Value bot_data = raw;
      if (!preprocess_bot_data(bot_data)) {
        error = u"preprocess_bot_data failed";
        return false;
      }
      const Value id = field_or(item, u"id");
      map_set(bot_map, id, bot_data);
      // 与 bot_map 的键保持一致：索引里的 id、文件名、数据自身的 id 都能查到。
      const bot::BotData typed_bot(bot_data);
      bot_datas[to_string(id)] = typed_bot;
      if (!equals(id, Value(file))) {
        map_set(bot_map, Value(file), bot_data);
        bot_datas[file] = typed_bot;
      }
      const Value bot_id = field_or(bot_data, u"id");
      if (!equals(id, bot_id)) {
        map_set(bot_map, bot_id, bot_data);
        bot_datas[to_string(bot_id)] = typed_bot;
      }
      list_ref(u"bots").push_back(bot_data);
    }

    for (size_t i = 0; i < moves_list->size(); ++i) {
      const Value item = moves_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      const std::u16string file = to_string(field_or(item, u"file"));
      report(Value(file));
      ImportResult result;
      Value raw;
      if (resources.import_json(file, true, result, error)) {
        raw = result.data;
      } else {
        host->warn({Value(u"FAILED TO LOAD MOVE LIST DATA: " + file)});
        error.clear();
      }
      if (!check_cancelled(error)) return false;
      if (!truthy(raw)) continue;
      const Value id = field_or(item, u"id");
      map_set(moves_map, id, raw);
      if (!equals(id, Value(file))) map_set(moves_map, Value(file), raw);
      const Value oid = field_or(raw, u"oid");
      if (truthy(oid) && !equals(oid, id)) map_set(moves_map, oid, raw);
      list_ref(u"moves").push_back(raw);
    }

    for (size_t i = 0; i < objects_list->size(); ++i) {
      const Value item = objects_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      if (!check_cancelled(error)) return false;
      const Value id = field_or(item, u"id");
      const std::u16string file = to_string(field_or(item, u"file"));
      const Value alias = field_or(item, u"alias");
      report(Value(file));
      Value raw;
      if (ends_with(file, u".obj.xml") || ends_with(file, u".xml")) {
        ImportResult result;
        if (!resources.import_xml(file, true, result, error)) {
          error = u"fail to load obj: " + file + u", reason: Error: " + error;
          return false;
        }
        if (result.xml_root == nullptr) {
          error = u"fail to load obj: " + file + u", reason: Error: missing xml root";
          return false;
        }
        raw = dat_translator::xml::xml_2_entity_data(result.xml_root.get());
      } else {
        ImportResult result;
        if (!resources.import_json(file, true, result, error)) {
          error = u"fail to load obj: " + file + u", reason: Error: " + error;
          return false;
        }
        raw = result.data;
      }
      Value cooked;
      if (!cook_data(raw, cooked, error)) {
        error = u"fail to load obj: " + file + u", reason: Error: " + error;
        return false;
      }
      if (!add_object(id, cooked, error)) {
        error = u"fail to load obj: " + file + u", reason: TypeError: " + error;
        return false;
      }
      if (!equals(id, Value(file))) {
        if (!add_object(Value(file), cooked, error)) {
          error = u"fail to load obj: " + file + u", reason: TypeError: " + error;
          return false;
        }
      }
      const Value cooked_id = field_or(cooked, u"id");
      if (!equals(id, cooked_id)) {
        if (!add_object(cooked_id, cooked, error)) {
          error = u"fail to load obj: " + file + u", reason: TypeError: " + error;
          return false;
        }
      }
      if (truthy(alias)) add_alias_object(alias, cooked);
    }

    for (size_t i = 0; i < bgs_list->size(); ++i) {
      const Value item = bgs_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      if (!check_cancelled(error)) return false;
      const std::u16string file = to_string(field_or(item, u"file"));
      report(Value(file));
      Value raw;
      if (ends_with(file, u".bg.xml")) {
        ImportResult result;
        if (!resources.import_xml(file, true, result, error)) {
          error = u"fail to load bg: " + file + u", reason: Error: " + error;
          return false;
        }
        if (result.xml_root == nullptr) {
          error = u"fail to load bg: " + file + u", reason: Error: missing xml root";
          return false;
        }
        raw = dat_translator::xml::xml_2_bg_data(*result.xml_root);
      } else {
        ImportResult result;
        if (!resources.import_json(file, true, result, error)) {
          error = u"fail to load bg: " + file + u", reason: Error: " + error;
          return false;
        }
        raw = result.data;
      }
      Value cooked;
      if (!cook_data(raw, cooked, error)) {
        error = u"fail to load bg: " + file + u", reason: Error: " + error;
        return false;
      }
      if (!add_bg(cooked, error)) {
        error = u"fail to load bg: " + file + u", reason: TypeError: " + error;
        return false;
      }
    }

    std::vector<Value> new_stages;
    for (size_t i = 0; i < stages_list->size(); ++i) {
      const Value item = stages_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      const std::u16string file = to_string(field_or(item, u"file"));
      report(Value(file));
      std::vector<Value> stage_datas;
      if (ends_with(file, u".xml") || ends_with(file, u".stage.xml")) {
        ImportResult result;
        if (!resources.import_xml(file, true, result, error)) return false;
        if (result.xml_root == nullptr) {
          error = u"missing xml root";
          return false;
        }
        stage_datas = dat_translator::xml::xml_to_stage_info_list(*result.xml_root);
      } else {
        ImportResult result;
        if (resources.import_json(file, true, result, error)) {
          if (const Array* const a = as_array(result.data)) {
            for (size_t j = 0; j < a->size(); ++j) stage_datas.push_back(a->at(j));
          } else if (!is_nullish(result.data)) {
            error = u"spread of non-iterable";
            return false;
          }
        } else {
          host->warn({Value(u"FAILED TO LOAD STATE: " + file)});
          error.clear();
        }
      }
      if (!check_cancelled(error)) return false;
      for (const Value& stage : stage_datas) {
        Value v = stage;
        new_stages.push_back(preprocess_stage(v));
      }
    }

    if (stages.empty()) {
      if (const Value* const void_stage = defines::find(u"Defines.VOID_STAGE")) {
        stages.insert(stages.begin(), *void_stage);
      }
    }
      for (const Value& stage : new_stages) {
      const Value sid = field_or(stage, u"id");
      stage_datas[to_string(sid)] = StageData(stage);
      const size_t idx = find_index_by_id(stages, sid);
      (void)check_stage_info(stage);
      if (idx == kNpos) {
        stages.push_back(stage);
      } else {
        stages[idx] = stage;
      }
    }

    for (size_t i = 0; i < directors_list->size(); ++i) {
      const Value item = directors_list->at(i);
      loaded += 1;
      if (truthy(field_or(item, u"skipped"))) continue;
      const std::u16string file = to_string(field_or(item, u"file"));
      report(Value(file));
      Value raw;
      if (ends_with(file, u".xml")) {
        ImportResult result;
        if (!resources.import_xml(file, true, result, error)) {
          host->warn({Value(u"FAILED TO LOAD DIRECTOR DATA: " + file)});
          error.clear();
          continue;
        }
        if (result.xml_root == nullptr) {
          host->warn({Value(u"FAILED TO LOAD DIRECTOR DATA: " + file)});
          continue;
        }
        const std::vector<Value> list = dat_translator::xml::xml_to_directors(*result.xml_root);
        Value arr(std::make_shared<Array>(list));
        raw = arr;
      } else {
        ImportResult result;
        if (!resources.import_json(file, true, result, error)) {
          host->warn({Value(u"FAILED TO LOAD DIRECTOR DATA: " + file)});
          error.clear();
          continue;
        }
        raw = result.data;
      }
      if (!check_cancelled(error)) return false;
      list_ref(u"directors").push_back(
          loader::make_director_group(item, raw, to_string(field_or(item, u"file"))));
    }
    return true;
  }
};

DatMgr::DatMgr(LFW* host) : _host(host) {
  _inner = std::make_shared<Inner>(this, ++_inner_id);
}

bool DatMgr::load(const std::vector<std::u16string>& index_files, std::u16string& error) {
  const std::shared_ptr<Inner> inner = _inner;
  return inner->load(index_files, error);
}

void DatMgr::dispose() { _inner = std::make_shared<Inner>(this, ++_inner_id); }

void DatMgr::clear() { _inner = std::make_shared<Inner>(this, ++_inner_id); }

const std::vector<Value>& DatMgr::directors() const { return _inner->list_ref(u"directors"); }

const Value* DatMgr::find_director(const Value& id) const {
  for (const Value& v : _inner->list_ref(u"directors")) {
    if (strict_equals(field_or(v, u"id"), id) || strict_equals(field_or(v, u"file"), id)) return &v;
  }
  return nullptr;
}

const std::vector<Value>& DatMgr::bots() const { return _inner->list_ref(u"bots"); }

const std::vector<Value>& DatMgr::moves() const { return _inner->list_ref(u"moves"); }

const std::vector<Value>& DatMgr::objects() const { return _inner->list_ref(u"objects"); }

const std::vector<Value>& DatMgr::fighters() const { return _inner->list_ref(u"8"); }

const std::vector<Value>& DatMgr::weapons() const { return _inner->list_ref(u"16"); }

const std::vector<Value>& DatMgr::balls() const { return _inner->list_ref(u"32"); }

const std::vector<Value>& DatMgr::entities() const { return _inner->list_ref(u"4"); }

const std::vector<Value>& DatMgr::backgrounds() const {
  return _inner->list_ref(u"background");
}

const std::vector<Value>& DatMgr::stages() const { return _inner->stages; }

const Value* DatMgr::find(const Value& id) const {
  if (const Value* const a = map_get_ptr(_inner->alias_map, id)) return a;
  return map_get_ptr(_inner->data_map, id);
}

const Value* DatMgr::find_bot(const Value& id) const { return map_get_ptr(_inner->bot_map, id); }

const Value* DatMgr::find_moves(const Value& id) const {
  return map_get_ptr(_inner->moves_map, id);
}

Randoming::Ptr DatMgr::get_randoming_by_group(const std::u16string& group) {
  Inner& inner = *_inner;
  for (const std::pair<std::u16string, Randoming::Ptr>& kv : inner.randomings) {
    if (kv.first == group) return kv.second;
  }
  const std::vector<Value> objects = get_objects_of_group(group);
  Randoming::Ptr ret = std::make_shared<Randoming>(u"dat_${group}_randoming", objects, &_host->mt_ref());
  inner.randomings.push_back(std::make_pair(group, ret));
  return ret;
}

const Value* DatMgr::find_weapon(const Value& id) const { return find_by_id(weapons(), id); }

const Value* DatMgr::find_weapon(const FindPredicate& predicate) const {
  return find_by_predicate(weapons(), predicate);
}

const Value* DatMgr::find_entity(const Value& id) const { return find_by_id(entities(), id); }

const Value* DatMgr::find_entity(const FindPredicate& predicate) const {
  return find_by_predicate(entities(), predicate);
}

const Value* DatMgr::find_object(const Value& id) const { return find_by_id(objects(), id); }

const Value* DatMgr::find_object(const FindPredicate& predicate) const {
  return find_by_predicate(objects(), predicate);
}

const bot::BotData* DatMgr::find_bot_data(const Value& id) const {
  if (const Object* const o = as_object(id)) {
    if (const Value* const idv = o->get(u"id")) {
      const auto it = _inner->bot_datas.find(to_string(*idv));
      if (it != _inner->bot_datas.end() && same_ref(it->second.to_value(), id)) return &it->second;
    }
    for (const std::pair<const std::u16string, bot::BotData>& kv : _inner->bot_datas) {
      if (same_ref(kv.second.to_value(), id)) return &kv.second;
    }
    return nullptr;
  }
  const auto it = _inner->bot_datas.find(to_string(id));
  return it != _inner->bot_datas.end() ? &it->second : nullptr;
}

const EntityData* DatMgr::find_entity_data(const Value& id) const {
  if (const Object* const o = as_object(id)) {
    if (const Value* const idv = o->get(u"id")) {
      const auto it = _inner->entity_datas.find(to_string(*idv));
      if (it != _inner->entity_datas.end() && same_ref(it->second.to_value(), id)) return &it->second;
    }
    for (const std::pair<const std::u16string, EntityData>& kv : _inner->entity_datas) {
      if (same_ref(kv.second.to_value(), id)) return &kv.second;
    }
    return nullptr;
  }
  const auto it = _inner->entity_datas.find(to_string(id));
  return it != _inner->entity_datas.end() ? &it->second : nullptr;
}

const StageData* DatMgr::find_stage_data(const Value& id) const {
  if (const Object* const o = as_object(id)) {
    if (const Value* const idv = o->get(u"id")) {
      const auto it = _inner->stage_datas.find(to_string(*idv));
      if (it != _inner->stage_datas.end() && same_ref(it->second.to_value(), id)) return &it->second;
    }
    for (const std::pair<const std::u16string, StageData>& kv : _inner->stage_datas) {
      if (same_ref(kv.second.to_value(), id)) return &kv.second;
    }
    return nullptr;
  }
  const auto it = _inner->stage_datas.find(to_string(id));
  return it != _inner->stage_datas.end() ? &it->second : nullptr;
}

const BackgroundData* DatMgr::find_bg_data(const Value& id) const {
  if (const Object* const o = as_object(id)) {
    if (const Value* const idv = o->get(u"id")) {
      const auto it = _inner->bg_datas.find(to_string(*idv));
      if (it != _inner->bg_datas.end() && same_ref(it->second.to_value(), id)) return &it->second;
    }
    for (const std::pair<const std::u16string, BackgroundData>& kv : _inner->bg_datas) {
      if (same_ref(kv.second.to_value(), id)) return &kv.second;
    }
    return nullptr;
  }
  const auto it = _inner->bg_datas.find(to_string(id));
  return it != _inner->bg_datas.end() ? &it->second : nullptr;
}

const bot::BotData* DatMgr::resolve_bot_data(const Value& data) const {
  if (const bot::BotData* const cached = find_bot_data(data)) return cached;
  for (const std::unique_ptr<bot::BotData>& one : _inner->ad_hoc_bot_datas) {
    if (same_ref(one->to_value(), data)) return one.get();
  }
  _inner->ad_hoc_bot_datas.push_back(std::make_unique<bot::BotData>(data));
  return _inner->ad_hoc_bot_datas.back().get();
}

const EntityData* DatMgr::resolve_entity_data(const Value& data) const {
  if (const EntityData* const cached = find_entity_data(data)) return cached;
  for (const std::unique_ptr<EntityData>& one : _inner->ad_hoc_entity_datas) {
    if (same_ref(one->to_value(), data)) return one.get();
  }
  _inner->ad_hoc_entity_datas.push_back(std::make_unique<EntityData>(data));
  return _inner->ad_hoc_entity_datas.back().get();
}

const StageData* DatMgr::resolve_stage_data(const Value& data) const {
  if (const StageData* const cached = find_stage_data(data)) return cached;
  for (const std::unique_ptr<StageData>& one : _inner->ad_hoc_stage_datas) {
    if (same_ref(one->to_value(), data)) return one.get();
  }
  _inner->ad_hoc_stage_datas.push_back(std::make_unique<StageData>(data));
  return _inner->ad_hoc_stage_datas.back().get();
}

const BackgroundData* DatMgr::resolve_bg_data(const Value& data) const {
  if (const BackgroundData* const cached = find_bg_data(data)) return cached;
  for (const std::unique_ptr<BackgroundData>& one : _inner->ad_hoc_bg_datas) {
    if (same_ref(one->to_value(), data)) return one.get();
  }
  _inner->ad_hoc_bg_datas.push_back(std::make_unique<BackgroundData>(data));
  return _inner->ad_hoc_bg_datas.back().get();
}

const Value* DatMgr::find_fighter(const Value& id) const { return find_by_id(fighters(), id); }

const Value* DatMgr::find_fighter(const FindPredicate& predicate) const {
  return find_by_predicate(fighters(), predicate);
}

const Value* DatMgr::find_background(const Value& id) const {
  return find_by_id(backgrounds(), id);
}

const Value* DatMgr::find_background(const FindPredicate& predicate) const {
  return find_by_predicate(backgrounds(), predicate);
}

std::vector<Value> DatMgr::get_objects_of_group(const std::u16string& group) const {
  std::vector<Value> out;
  for (const Value& v : objects()) {
    const Value g = field_or(field_or(v, u"base"), u"group");
    if (!truthy(g)) continue;
    if (js_index_of(g, Value(group)) >= 0) out.push_back(v);
  }
  return out;
}

std::vector<Value> DatMgr::get_fighters_of_group(const std::u16string& group) const {
  std::vector<Value> out;
  for (const Value& v : fighters()) {
    const Value g = field_or(field_or(v, u"base"), u"group");
    if (!truthy(g)) continue;
    if (js_index_of(g, Value(group)) >= 0) out.push_back(v);
  }
  return out;
}

std::vector<Value> DatMgr::get_weapons_of_group(const std::u16string& group) const {
  std::vector<Value> out;
  for (const Value& v : weapons()) {
    const Value g = field_or(field_or(v, u"base"), u"group");
    if (!truthy(g)) continue;
    if (js_index_of(g, Value(group)) >= 0) out.push_back(v);
  }
  return out;
}

std::vector<Value> DatMgr::get_fighters_not_in_group(const std::u16string& group) const {
  std::vector<Value> out;
  for (const Value& v : fighters()) {
    const Value g = field_or(field_or(v, u"base"), u"group");
    if (!truthy(g) || js_index_of(g, Value(group)) < 0) out.push_back(v);
  }
  return out;
}

std::vector<Value> DatMgr::get_backgrouds_of_group(const std::u16string& group) const {
  std::vector<Value> out;
  for (const Value& v : backgrounds()) {
    const Array* const a = as_array(field_or(field_or(v, u"base"), u"group"));
    if (a == nullptr) continue;
    for (size_t i = 0; i < a->size(); ++i) {
      if (strict_equals(a->at(i), Value(group))) {
        out.push_back(v);
        break;
      }
    }
  }
  return out;
}

Randoming::Ptr DatMgr::get_bg_randoming_of_group(const std::vector<std::u16string>& groups) {
  Inner& inner = *_inner;
  const std::u16string key = join_strings(groups, u",");
  for (const std::pair<std::u16string, Randoming::Ptr>& kv : inner.bg_randomings) {
    if (kv.first == key) return kv.second;
  }
  std::vector<Value> pool;
  for (const std::u16string& group : groups) {
    for (const Value& bg : get_backgrouds_of_group(group)) {
      bool seen = false;
      for (const Value& prev : pool) {
        if (same_set_key(prev, bg)) {
          seen = true;
          break;
        }
      }
      if (!seen) pool.push_back(bg);
    }
  }
  Randoming::Ptr ret = std::make_shared<Randoming>(u"bg_" + join_strings(groups, u"_") + u"_randoming",
                                                   pool, &_host->mt_ref());
  inner.bg_randomings.push_back(std::make_pair(key, ret));
  return ret;
}

Value DatMgr::get_random_bg(const std::vector<std::u16string>& groups) {
  return get_bg_randoming_of_group(groups)->get();
}

}
}
