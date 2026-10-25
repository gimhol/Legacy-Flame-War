#include "lfw/cmds/cmd_query.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lfw/bg/background.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/data/background_data.h"
#include "lfw/defines/game_key.h"
#include "lfw/entity/entity.h"
#include "lfw/lfw.h"
#include "lfw/loader/dat_mgr.h"
#include "lfw/player_info.h"
#include "lfw/stage/stage.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/world.h"

namespace lfw {
namespace cmds {

namespace {

constexpr size_t kColumnMin = 2;
constexpr size_t kColumnMax = 24;
constexpr size_t kFieldWidth = 11;  // INFO 左侧键名的宽度

std::u16string pad_right(const std::u16string& text, size_t width) {
  if (text.size() >= width) return text;
  std::u16string out = text;
  out.append(width - text.size(), u' ');
  return out;
}

std::u16string num(double v) { return number_to_string(v); }

std::u16string vec2(double x, double y) { return u"(" + num(x) + u"," + num(y) + u")"; }

std::u16string vec3(const Vector3& p) {
  return u"(" + num(p.x) + u"," + num(p.y) + u"," + num(p.z) + u")";
}

bool usable(const std::u16string& text) { return !text.empty() && text != u"undefined"; }

/** 数据对象里取一个可显示的标签：`name` → `title` → `base.name` */
std::u16string label_of(const Value& data) {
  const Object* const obj = as_object(data);
  if (obj == nullptr) return std::u16string();
  for (const char16_t* key : {u"name", u"title"}) {
    const Value* const v = obj->get(key);
    if (v != nullptr && std::holds_alternative<std::u16string>(*v) && usable(to_string(*v))) {
      return to_string(*v);
    }
  }
  const Value* const base = obj->get(u"base");
  const Object* const base_obj = base != nullptr ? as_object(*base) : nullptr;
  if (base_obj != nullptr) {
    const Value* const name = base_obj->get(u"name");
    if (name != nullptr && std::holds_alternative<std::u16string>(*name)) return to_string(*name);
  }
  return std::u16string();
}

std::u16string id_of(const Value& data) {
  const Object* const obj = as_object(data);
  if (obj == nullptr) return std::u16string();
  const Value* const id = obj->get(u"id");
  return id != nullptr ? to_string(*id) : std::u16string();
}

bool contains_ci(const std::u16string& haystack, const std::u16string& needle) {
  if (needle.empty()) return true;
  return to_lower_case(haystack).find(to_lower_case(needle)) != std::u16string::npos;
}

/** 一行一项：`id` 左对齐 + 名称（列宽随最长 id 伸缩） */
std::u16string rows_of(const std::vector<std::u16string>& ids,
                       const std::vector<std::u16string>& labels) {
  size_t longest = 0;
  for (const std::u16string& id : ids) longest = std::max(longest, id.size());
  const size_t width = std::clamp(longest + 2, kColumnMin, kColumnMax);
  std::u16string out;
  for (size_t i = 0; i < ids.size(); ++i) {
    out += u"\n  " + pad_right(ids[i], width);
    out += i < labels.size() ? labels[i] : std::u16string();
  }
  return out;
}

/** `LIST` 支持的数据表：名字 → 取数据 */
struct ListKind {
  const char16_t* name;
  const std::vector<Value>* (*get)(loader::DatMgr& mgr);
  bool alias = false;  // 同义名，报错列表里只列一次
};

const std::vector<ListKind>& list_kinds() {
  static const std::vector<ListKind> kinds = {
      {u"fighters", [](loader::DatMgr& m) { return &m.fighters(); }},
      {u"weapons", [](loader::DatMgr& m) { return &m.weapons(); }},
      {u"balls", [](loader::DatMgr& m) { return &m.balls(); }},
      {u"objects", [](loader::DatMgr& m) { return &m.objects(); }},
      {u"entities", [](loader::DatMgr& m) { return &m.entities(); }},
      {u"bots", [](loader::DatMgr& m) { return &m.bots(); }},
      {u"moves", [](loader::DatMgr& m) { return &m.moves(); }},
      {u"stages", [](loader::DatMgr& m) { return &m.stages(); }},
      {u"bg", [](loader::DatMgr& m) { return &m.backgrounds(); }},
      {u"backgrounds", [](loader::DatMgr& m) { return &m.backgrounds(); }, true},
      {u"directors", [](loader::DatMgr& m) { return &m.directors(); }},
  };
  return kinds;
}

std::u16string kind_names() {
  std::u16string out;
  for (const ListKind& kind : list_kinds()) {
    if (kind.alias) continue;
    if (!out.empty()) out += u"|";
    out += kind.name;
  }
  return out;
}

size_t node_count(const ui::UINode& node) {
  size_t n = 1;
  for (const ui::UINode* const child : node.children()) {
    if (child != nullptr) n += node_count(*child);
  }
  return n;
}

std::u16string node_label(const ui::UINode& node) {
  const std::u16string id = to_string(node.id());
  if (usable(id)) return id;
  const std::u16string name = to_string(node.name());
  return usable(name) ? name : std::u16string(u"?");
}

std::u16string node_line(const ui::UINode& node) {
  std::u16string line = node_label(node);
  if (!node.children().empty()) {
    line += u" (" + num(static_cast<double>(node.children().size())) + u")";
  }
  if (!node.visible()) line += u" [hidden]";
  if (node.disabled()) line += u" [disabled]";
  return line;
}

/** 节点树：每行 `下标 名字 (子节点数)`，缩进层级就是 `--paths` 的后续下标 */
void dump_tree(const ui::UINode& node, size_t depth, size_t max_depth, std::u16string& out) {
  if (depth >= max_depth) return;
  for (size_t i = 0; i < node.children().size(); ++i) {
    const ui::UINode* const child = node.children()[i];
    if (child == nullptr) continue;
    out += u"\n" + std::u16string((depth + 1) * 2, u' ') + num(static_cast<double>(i)) + u" " +
           node_line(*child);
    dump_tree(*child, depth + 1, max_depth, out);
  }
}

std::vector<Entity*> collect_entities(World& world, const std::u16string& team, bool with_all) {
  std::vector<Entity*> list;
  const auto take = [&](const std::vector<Entity*>& src) {
    for (Entity* const e : src) {
      if (e == nullptr) continue;
      if (!team.empty() && e->team() != team) continue;
      if (std::find(list.begin(), list.end(), e) != list.end()) continue;  // 傀儡也在 entities 里
      list.push_back(e);
    }
  };
  take(world.entities);
  if (!with_all) return list;
  take(world.ghosts);
  for (const std::pair<std::u16string, Entity*>& kv : world.puppets) {
    if (kv.second == nullptr) continue;
    if (!team.empty() && kv.second->team() != team) continue;
    if (std::find(list.begin(), list.end(), kv.second) != list.end()) continue;
    list.push_back(kv.second);
  }
  return list;
}

std::u16string entity_line(const Entity& e) {
  std::u16string line = u"#" + e.id;
  const std::u16string data_id = to_string(e.data_id());
  if (usable(data_id)) line += u" " + data_id;
  const std::u16string name = to_string(e.name());
  if (usable(name)) line += u" \"" + name + u"\"";
  line += u"  team=" + e.team();
  line += u"  hp=" + num(e.hp()) + u"/" + num(e.hp_max());
  line += u"  mp=" + num(e.mp()) + u"/" + num(e.mp_max());
  line += u"  pos=" + vec3(e.position);
  return line;
}

}  // namespace

const char16_t* cmd_list_help() {
  return uR"(Usage: LIST <kind> [filter]

List data ids of one kind; 'filter' matches id or name (case-insensitive)
列出某一类数据的所有 id；filter 按 id / 名称做（大小写不敏感的）子串匹配

Kinds: fighters|weapons|balls|objects|entities|bots|moves|stages|bg|directors)";
}

void cmd_list(CMDS& ctx) {
  LFW& lfw = ctx.world().lfw();
  const std::optional<std::u16string> which = ctx.str(1);
  if (!which.has_value() || which->empty()) {
    lfw.warn(u"LIST failed, must \"LIST <kind> [filter]\", kinds: " + kind_names() +
             u", got: " + ctx.cmd());
    return;
  }
  const std::u16string want = to_lower_case(*which);
  const ListKind* hit = nullptr;
  for (const ListKind& kind : list_kinds()) {
    if (want == kind.name) {
      hit = &kind;
      break;
    }
  }
  if (hit == nullptr) {
    lfw.warn(u"LIST failed, unknown kind \"" + *which + u"\", kinds: " + kind_names() +
             u"（用 `help list` 看说明）");
    return;
  }

  const std::vector<Value>& all = *hit->get(lfw.datas());
  const std::u16string filter = ctx.str(2).value_or(std::u16string());
  std::vector<std::u16string> ids;
  std::vector<std::u16string> labels;
  for (const Value& data : all) {
    const std::u16string id = id_of(data);
    const std::u16string label = label_of(data);
    if (!contains_ci(id, filter) && !contains_ci(label, filter)) continue;
    ids.push_back(id);
    labels.push_back(label == id ? std::u16string() : label);
  }

  std::u16string head = hit->name;
  head += u" (" + num(static_cast<double>(ids.size()));
  if (!filter.empty()) head += u"/" + num(static_cast<double>(all.size()));
  head += u"):";
  if (ids.empty()) {
    lfw.warn(head + u"（没有匹配 \"" + filter + u"\" 的项）");
    return;
  }
  lfw.log(head + rows_of(ids, labels));
}

const char16_t* cmd_entities_help() {
  return uR"(Usage: ENTITIES [--team=<team>] [--all]

List live entities: id, data id, team, hp/mp, position
列出场上实体：id、数据 id、队伍、血量/魔力、坐标

Options:
  --team=<team>   only that team / 只看这一队
  --all           include ghosts and puppets / 连残影与傀儡一起列)";
}

void cmd_entities(CMDS& ctx) {
  LFW& lfw = ctx.world().lfw();
  const std::u16string team = ctx.str_arg(u"--team").value_or(std::u16string());
  const bool with_all = ctx.str_arg(u"--all").has_value();
  const std::vector<Entity*> list = collect_entities(ctx.world(), team, with_all);

  if (list.empty()) {
    lfw.log(u"entities (0):" +
            (team.empty() ? std::u16string() : u"（没有 team=" + team + u" 的实体）"));
    return;
  }
  std::u16string out = u"entities (" + num(static_cast<double>(list.size())) + u"):";
  for (const Entity* const e : list) out += u"\n  " + entity_line(*e);
  lfw.log(out);
}

const char16_t* cmd_info_help() {
  return uR"(Usage: INFO

Show the current world summary: stage, background, difficulty, entities, players
显示当前世界概要：关卡、背景、难度、实体数、玩家)";
}

void cmd_info(CMDS& ctx) {
  World& world = ctx.world();
  LFW& lfw = world.lfw();

  std::u16string stage = to_string(world.stage_value(u"id"));
  if (!usable(stage)) stage = u"-";
  if (const Stage* const s = world.stage(); s != nullptr && !s->name().empty()) {
    stage += u" (" + s->name() + u")";
  }

  std::u16string bg = u"-";
  if (const Background* const b = world.bg(); b != nullptr) {
    bg = to_string(b->id());
    if (!usable(bg)) bg = u"-";
    const std::u16string name = to_string(b->name());
    if (usable(name)) bg += u" (" + name + u")";
  }

  std::u16string players;
  for (const std::pair<std::u16string, std::unique_ptr<PlayerInfo>>& kv : lfw.players()) {
    if (kv.second == nullptr || kv.second->fighter() == nullptr) continue;
    if (!players.empty()) players += u", ";
    players += kv.first;
    const std::u16string name = to_string(kv.second->name());
    if (usable(name) && name != kv.first) players += u"(" + name + u")";
    if (kv.second->is_com()) players += u"[com]";
  }
  if (players.empty()) players = u"-";

  std::u16string camera = vec2(world.camera().position.x, world.camera().position.y);
  if (world.camera().locked() != nullptr) camera += u" locked";

  std::vector<std::u16string> lines;
  const auto add = [&](const char16_t* key, const std::u16string& value) {
    lines.push_back(pad_right(key, kFieldWidth) + value);
  };
  add(u"stage", stage);
  add(u"bg", bg);
  add(u"difficulty", num(to_number(world.difficulty())));
  add(u"paused", world.paused() ? u"true" : u"false");
  add(u"camera", camera);
  add(u"entities", num(static_cast<double>(world.entities.size())) + u"  ghosts=" +
                       num(static_cast<double>(world.ghosts.size())) + u"  puppets=" +
                       num(static_cast<double>(world.puppets.size())));
  add(u"players", players);
  add(u"lang", lfw.lang());
  add(u"version", LFW::VERSION_NAME());

  std::u16string out;
  for (const std::u16string& line : lines) {
    if (!out.empty()) out += u"\n";
    out += line;
  }
  lfw.log(out);
}

namespace {

bool missing(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

std::u16string value_text(const Value& v) { return missing(v) ? std::u16string(u"-") : to_string(v); }

/** `#101` 与 `101` 都认；残影 / 傀儡不在 `entity_map` 里，再兜一圈 */
Entity* find_entity_any(World& world, const std::u16string& raw) {
  const std::u16string id = !raw.empty() && raw[0] == u'#' ? raw.substr(1) : raw;
  if (id.empty()) return nullptr;
  if (Entity* const e = world.find_entity(id); e != nullptr) return e;
  for (Entity* const e : world.ghosts) {
    if (e != nullptr && e->id == id) return e;
  }
  for (const std::pair<std::u16string, Entity*>& kv : world.puppets) {
    if (kv.second != nullptr && kv.second->id == id) return kv.second;
  }
  return nullptr;
}

}  // namespace

const char16_t* cmd_dataset_help() {
  return uR"(Usage: DATASET [<key>] [--entity=<id>]

No <key>: list the current background's dataset (the map's own gravity / gravity_d …)
With <key>: print that key's layers (frame → data → bg → world) and the value in use
不写 <key>：列当前背景的数据集（地图自己的 gravity / gravity_d …）
给 <key>：按 frame → 实体数据 → 背景 → 世界 打印这一键的各层值与真正生效的值

Options:
  --entity=<id>   include that entity's layers + the value it actually uses / 连实体那一层一起看)";
}

void cmd_dataset(CMDS& ctx) {
  World& world = ctx.world();
  LFW& lfw = world.lfw();
  const std::optional<std::u16string> key = ctx.str(1);
  const std::optional<std::u16string> entity_arg = ctx.str_arg(u"--entity");

  const Background* const bg = world.bg();
  const KeyedMap* const bg_ds =
      bg != nullptr && bg->data() != nullptr ? &bg->data()->dataset() : nullptr;
  std::u16string bg_label = u"-";
  if (bg != nullptr) {
    bg_label = value_text(bg->id());
    const std::u16string name = to_string(bg->name());
    if (usable(name) && name != bg_label) bg_label += u" (" + name + u")";
  }

  if (!key.has_value() || key->empty()) {
    std::u16string out;
    if (bg_ds == nullptr) {
      out = u"bg dataset: -（当前没有背景）";
    } else {
      const std::vector<std::u16string> keys = bg_ds->keys();
      out = u"bg " + bg_label + u" dataset (" + num(static_cast<double>(keys.size())) + u"):";
      for (const std::u16string& k : keys) {
        const std::u16string label =
            k.size() < kFieldWidth ? pad_right(k, kFieldWidth) : k + u"  ";
        out += u"\n  " + label + value_text(bg_ds->get(k));
      }
      if (keys.empty()) out += u"\n  （空：这个地图没有覆盖任何 world dataset 字段）";
    }
    out += u"\nworld dataset: " + num(static_cast<double>(world.dataset.keys().size())) +
           u" keys（默认值；`DATASET <key> [--entity=#101]` 看单个键的解析链）";
    lfw.log(out);
    return;
  }

  const std::u16string k = *key;
  Entity* entity = nullptr;
  if (entity_arg.has_value() && !entity_arg->empty()) {
    entity = find_entity_any(world, *entity_arg);
    if (entity == nullptr) {
      lfw.warn(u"DATASET failed, no such entity: " + *entity_arg +
               u"（`ENTITIES --all` 看有哪些）");
      return;
    }
  }

  const Value world_v = world.dataset.get(k);
  const Value bg_v = bg_ds != nullptr ? bg_ds->get(k) : Value();
  Value data_v;
  Value frame_v;
  if (entity != nullptr) {
    if (const Value* const p = entity->data_base().get(k)) data_v = *p;
    if (entity->frame != nullptr) frame_v = entity->frame->dataset().get(k);
  }

  // 解析顺序与 `Entity::dataset` 一致：frame → data → bg → world
  Value effective = world_v;
  const char16_t* source = u"world";
  if (!missing(bg_v)) {
    effective = bg_v;
    source = u"bg";
  }
  if (!missing(data_v)) {
    effective = data_v;
    source = u"data";
  }
  if (!missing(frame_v)) {
    effective = frame_v;
    source = u"frame";
  }

  std::u16string out = k + u":";
  const auto add = [&](const char16_t* label, const std::u16string& value) {
    out += u"\n  " + pad_right(label, kFieldWidth) + value;
  };
  add(u"frame", value_text(frame_v));
  add(u"data", value_text(data_v));
  add(u"bg", value_text(bg_v) + (missing(bg_v) ? std::u16string() : u"  " + bg_label));
  add(u"world", value_text(world_v));
  add(u"effective", value_text(effective) + u"  <- " + source);
  if (entity != nullptr) {
    std::u16string ent = u"#" + entity->id;
    if (k == u"gravity" || k == u"gravity_d") {
      const bool defending = entity->ctrl_is_end(std::u16string(gk::kDefend));
      ent += u"  gravity()=" + num(entity->gravity());
      ent += u"  defending=" + num(defending ? 1.0 : 0.0);
      ent += u"  key=" + std::u16string(defending ? u"gravity" : u"gravity_d");
    } else {
      ent += u"  dataset=" + value_text(entity->dataset(k));
    }
    add(u"entity", ent);
  }
  lfw.log(out);
}

const char16_t* cmd_ui_help() {
  return uR"(Usage: UI [layer] [--depth=<n>]

List UI layers, or one layer's node tree — its child indices are what POINTER_* '--paths' takes
列出 UI 各层，或某一层的节点树（子节点下标就是 POINTER_* 的 --paths 要的序号）

Options:
  --depth=<n>   tree depth of `UI <layer>`, default 3 / 节点树深度，默认 3)";
}

void cmd_ui(CMDS& ctx) {
  LFW& lfw = ctx.world().lfw();
  ui::UILayers* const layers = lfw.ui_layers();
  if (layers == nullptr) {
    lfw.warn(u"UI failed, ui not loaded");
    return;
  }
  const std::vector<ui::UILayer*> all = layers->all();
  const std::optional<std::u16string> which = ctx.str(1);

  if (!which.has_value() || which->empty()) {
    std::u16string out = u"ui layers (" + num(static_cast<double>(all.size())) + u"):";
    for (const ui::UILayer* const layer : all) {
      if (layer == nullptr) continue;
      const ui::UINode* const page = layer->ui();
      out += u"\n  [" + num(layer->index()) + u"] ";
      out += page != nullptr ? node_label(*page) : std::u16string(u"-");
      out += u"  pages=" + num(static_cast<double>(layer->pages().size()));
      out += u"  nodes=" + num(page != nullptr ? static_cast<double>(node_count(*page)) : 0.0);
    }
    if (!all.empty()) out += u"\n（`ui <layer>` 看节点树，`--depth=<n>` 调深度）";
    lfw.log(out);
    return;
  }

  const double index = string_to_number(*which);
  const ui::UILayer* found = nullptr;
  for (const ui::UILayer* const layer : all) {
    if (layer != nullptr && layer->index() == index) found = layer;
  }
  if (found == nullptr) {
    lfw.warn(u"UI failed, layer not found: " + *which + u"（用 `ui` 看有哪些层）");
    return;
  }
  const ui::UINode* const page = found->ui();
  if (page == nullptr) {
    lfw.warn(u"UI failed, layer " + *which + u" has no page");
    return;
  }
  const double depth_arg = ctx.num_arg(u"--depth").value_or(3.0);
  const size_t max_depth = static_cast<size_t>(std::clamp(depth_arg, 0.0, 32.0));
  std::u16string out = u"[" + num(found->index()) + u"] " + node_line(*page);
  dump_tree(*page, 0, max_depth, out);
  lfw.log(out);
}

}  // namespace cmds
}  // namespace lfw
