#include "lfw/stage/stage.h"

#include "lfw/world.h"
#include "lfw/lfw.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/js_num.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/team_enum.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/stage/status.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/round_float.h"
#include "lfw/utils/type_check.h"

namespace lfw {
namespace stage {
namespace {

bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool same_ref(const Value& a, const Value& b) {
  const Object* const oa = as_object(a);
  const Object* const ob = as_object(b);
  if (oa != nullptr || ob != nullptr) return oa == ob;
  const Array* const aa = as_array(a);
  const Array* const ab = as_array(b);
  if (aa != nullptr || ab != nullptr) return aa == ab;
  return strict_equals(a, b);
}

bool set_has(const std::vector<Value>& set, const Value& v) {
  for (const Value& x : set) {
    if (strict_equals(x, v)) return true;
  }
  return false;
}

void set_add(std::vector<Value>& set, const Value& v) {
  if (!set_has(set, v)) set.push_back(v);
}

double or_zero(const Value& v) { return truthy(v) ? to_number(v) : 0.0; }

class StageRunningState : public IState {
 public:
  explicit StageRunningState(Stage* stage) : _stage(stage) {}

  Value key() const override { return Value(std::u16string(status::kRunning)); }
  std::optional<Value> update(double dt) override {
    (void)dt;
    if (_stage->is_stage_finish()) return Value(std::u16string(status::kCompleted));
    return std::nullopt;
  }

 private:
  Stage* _stage = nullptr;
};

class StageCompletedState : public IState {
 public:
  explicit StageCompletedState(Stage* stage) : _stage(stage) {}

  Value key() const override { return Value(std::u16string(status::kCompleted)); }
  void enter() override {
    _stage->callbacks().call(u"on_stage_finish", {StageCallbackArgs{_stage, Value(), Value()}});
    if (_stage->is_chapter_finish()) {
      _stage->callbacks().call(u"on_chapter_finish", {StageCallbackArgs{_stage, Value(), Value()}});
    }
  }
  std::optional<Value> update(double dt) override {
    (void)dt;
    if (_stage->should_goto_next_stage()) {
      _stage->callbacks().call(u"on_requrie_goto_next_stage",
                               {StageCallbackArgs{_stage, Value(), Value()}});
      return Value(std::u16string(status::kEnd));
    }
    return std::nullopt;
  }

 private:
  Stage* _stage = nullptr;
};

class StageEndState : public IState {
 public:
  Value key() const override { return Value(std::u16string(status::kEnd)); }
};

}

Stage::Warn& stage_warn_sink() {
  static Stage::Warn warn;
  return warn;
}

void Stage::set_warn(Warn warn) { stage_warn_sink() = std::move(warn); }

void Stage::warn(const std::u16string& where, const std::u16string& text) {
  if (stage_warn_sink()) stage_warn_sink()(where, text);
}

Background* Stage::bg() const { return _world->bg(); }
Value Stage::difficulty() const { return _world->difficulty(); }

MersenneTwister* Stage::mt() { return _lfw->mt(); }
Value Stage::datas_find(const Value& oid) { return _lfw->datas_find(oid); }
std::shared_ptr<Randoming> Stage::datas_randoming_by_group(const Value& oid) {
  return _lfw->datas_randoming_by_group(oid);
}
Entity* Stage::create_entity_with_bot(const Value& data) {
  return _lfw->create_entity_with_bot(data);
}

Stage::Stage(World* world, LFW* lfw, const StageData& data)
    : _world(world), _lfw(lfw), _data(data) {
  _states.push_back(std::make_unique<StageRunningState>(this));
  _states.push_back(std::make_unique<StageCompletedState>(this));
  _states.push_back(std::make_unique<StageEndState>());
  _fsm.add(_states[0].get(), _states[1].get(), _states[2].get());
  _fsm.use(Value(std::u16string(status::kRunning)));

  const std::optional<std::u16string>& bid_opt = _data.bg();
  const Value bid = bid_opt.has_value() ? Value(*bid_opt) : Value();
  Value bdt = _lfw->datas_backgrounds_find(bid);
  if (!truthy(bdt)) warn(u"Stage::constructor", u"bg not found, id: " + to_string(bid));
  if (is_nullish(bdt)) {
    const Value* const void_bg = defines::find(u"Defines.VOID_BG");
    bdt = void_bg != nullptr ? *void_bg : Value();
  }
  change_bg(bdt);

  Background* const b = _world->bg();
  left = cam_l = player_l = enemy_l = b->left();
  right = cam_r = player_r = enemy_r = b->right();
  _near = b->near_plane();
  _far = b->far_plane();
  width = b->width();
  depth = b->depth();
  middle = b->middle();
  drink_l = -1200.0;
  drink_r = _world->bg()->width() + 1200.0;
  const Value next = _data.next().has_value() ? Value(*_data.next()) : Value();
  if (truthy(next)) _next_stage = _lfw->datas_stages_find(next);
  _team = _lfw->new_team();
}

Stage::~Stage() = default;

Background* Stage::change_bg(const Value& data) {
  Background* const prev_bg = _world->bg();
  if (prev_bg != nullptr) {
    if (strict_equals(Value(prev_bg->data().id()), field_or(data, u"id"))) return prev_bg;
  }
  if (prev_bg != nullptr) prev_bg->dispose();
  Stage* const world_stage = _world->stage();
  Background* const curr_bg = world_stage != nullptr ? _world->bg() : nullptr;
  if (curr_bg != nullptr &&
      strict_equals(Value(curr_bg->data().id()), field_or(data, u"id"))) {
    return curr_bg;
  }

  auto bg = std::make_unique<Background>(_world, _lfw->datas().resolve_bg_data(data));
  left = cam_l = player_l = enemy_l = bg->left();
  right = cam_r = player_r = enemy_r = bg->right();
  _near = bg->near_plane();
  _far = bg->far_plane();
  width = bg->width();
  depth = bg->depth();
  middle = bg->middle();
  drink_l = kMinSafeInteger;
  drink_r = kMaxSafeInteger;
  Background* const created = bg.get();
  _world->set_bg(std::move(bg));
  return created;
}

Value Stage::title() const {
  const std::optional<std::u16string>& t = _data.title();
  if (t.has_value()) return Value(*t);
  return _world->bg()->name();
}

const std::vector<StagePhaseInfo>& Stage::phases() const { return _data.phases(); }

std::u16string Stage::id() const { return _data.id(); }

std::u16string Stage::name() const { return _data.name(); }

const DialogInfo* Stage::dialog() const {
  if (_dialogs.index < 0.0) return nullptr;
  const size_t idx = static_cast<size_t>(_dialogs.index);
  if (idx >= _dialogs.list.size()) return nullptr;
  return &_dialogs.list[idx];
}

bool Stage::world_pause() const { return truthy(Value(_phase_info.world_pause())); }

bool Stage::control_disabled() const { return truthy(Value(_phase_info.control_disabled())); }

bool Stage::weapon_rain_disabled() const {
  return truthy(Value(_phase_info.weapon_rain_disabled()));
}

void Stage::stop_bgm() {
  if (_stop_bgm) _stop_bgm();
}

Value Stage::dialog_state_value(const DialogState& s) const {
  auto o = std::make_shared<Object>();
  o->set(u"index", Value(s.index));
  auto list = std::make_shared<Array>();
  for (const DialogInfo& v : s.list) list->push_back(v.to_value());
  o->set(u"list", Value(list));
  return Value(o);
}

void Stage::play_phase_sounds() {
  if (!_phase_info.valid()) return;
  const std::optional<std::u16string>& music = _phase_info.music();
  if (music.has_value()) {
    if (!music->empty()) {
      _stop_bgm = _lfw->sounds_play_bgm(Value(*music));
    } else {
      _stop_bgm = nullptr;
      _lfw->sounds_stop_bgm();
    }
  }
  for (const SoundPlayInfo& s : _phase_info.sounds()) {
    _lfw->sounds_play(Value(s.path()), Value(s.x()), Value(s.y()), Value(s.z()));
  }
}

void Stage::set_phase(const StagePhaseInfo* phase) {
  if (phase != nullptr && same_ref(phase->to_value(), _phase)) return;
  phase_time = 0.0;
  phase_end_tester.reset(
      _lfw->end_testers(phase != nullptr ? phase->end_test() : std::vector<std::u16string>()));

  const Value prev = _phase;
  _phase = phase != nullptr ? phase->to_value() : Value();
  _phase_info = phase != nullptr ? *phase : StagePhaseInfo();
  _callbacks.call(u"on_phase_changed", {StageCallbackArgs{this, _phase, prev}});
  player_l = 0.0;
  player_r = _world->bg()->right();
  if (phase == nullptr) return;

  const KeyedMap& respawn = phase->respawn();
  const KeyedMap& respawn_r = phase->respawn_r();
  const KeyedMap& health_up = phase->health_up();
  const KeyedMap& mp_up = phase->mp_up();
  const KeyedMap& respawn_x = phase->respawn_x();

  const Value difficulty = _world->difficulty();
  const std::u16string difficulty_key = to_string(difficulty);
  auto map_at = [&](const KeyedMap& map_obj) -> Value {
    if (!map_obj.valid()) return Value();
    return map_obj.get(difficulty_key);
  };
  const Value hp_recovery_v = map_at(health_up);
  const double hp_recovery = truthy(hp_recovery_v) ? to_number(hp_recovery_v) : 0.0;
  const Value hp_recovery_r_v = map_at(health_up);
  const double hp_recovery_r = truthy(hp_recovery_r_v) ? to_number(hp_recovery_r_v) : hp_recovery;
  const Value hp_respawn_v = map_at(respawn);
  const double hp_respawn = truthy(hp_respawn_v) ? to_number(hp_respawn_v) : 0.0;
  const Value hp_respawn_r_v = map_at(respawn_r);
  const double hp_respawn_r = truthy(hp_respawn_r_v) ? to_number(hp_respawn_r_v) : hp_respawn;
  const double mp_recovery = or_zero(map_at(mp_up));
  const Value _respawn_x = map_at(respawn_x);

  const bool loop_players_fighters =
      truthy(Value(hp_recovery)) || truthy(Value(hp_respawn)) || truthy(Value(mp_recovery));
  if (loop_players_fighters) {
    std::vector<Value> teams;
    for (Entity* const f : _world->puppet_list()) set_add(teams, Value(f->team()));
    for (Entity* const f : _world->entity_list()) {
      if (!entity::is_fighter(*f) || !set_has(teams, Value(f->team()))) continue;
      if (f->hp() <= 0.0 && truthy(Value(hp_respawn))) {
        const double hp = hp_respawn < 1.0 ? lfw::min(f->hp_max() * hp_respawn, f->hp_max())
                                           : lfw::min(hp_respawn, f->hp_max());
        f->set_hp(hp);
        const double hp_r = hp_respawn < 1.0 ? lfw::min(f->hp_max() * hp_respawn_r, f->hp_max())
                                             : lfw::min(hp_respawn_r, f->hp_max());
        f->set_hp_r(lfw::max(hp_r, hp));
        if (is_num(_respawn_x)) f->set_position(_respawn_x, Value(NullTag{}), Value(NullTag{}));
      } else if (f->hp() > 0.0 && truthy(Value(hp_recovery))) {
        const double hp = hp_recovery < 1.0
                              ? lfw::min(f->hp_r() + (f->hp_max() - f->hp_r()) * hp_recovery,
                                         f->hp_max())
                              : lfw::min(f->hp_r() + hp_recovery, f->hp_max());
        f->set_hp(hp);
        const double hp_r = hp_recovery_r < 1.0
                                ? lfw::min(f->hp_r() + (f->hp_max() - f->hp_r()) * hp_recovery,
                                           f->hp_max())
                                : lfw::min(f->hp_r() + hp_recovery, f->hp_max());
        f->set_hp_r(lfw::max(hp_r, hp));
      }
      if (truthy(Value(mp_recovery))) {
        f->set_mp(lfw::min(f->mp() + mp_recovery, f->mp_max()));
      }
    }
  }

  play_phase_sounds();
  const double ce_count = ce();
  for (const StageObjectInfo& obj : phase->objects()) spawn(*phase, obj, ce_count);
  if (!std::isnan(phase->cam_jump_to_x())) _world->camera_jump_x(Value(phase->cam_jump_to_x()));

  const auto or_default = [](double v, double fallback) {
    return std::isnan(v) ? fallback : v;
  };
  player_l = or_default(phase->player_l(), 0.0);
  cam_l = or_default(phase->camera_l(), 0.0);
  enemy_l = or_default(phase->enemy_l(), -1200.0);
  drink_l = or_default(phase->drink_l(), -1200.0);
  const double bound = phase->bound();
  auto bound_or_bg_right = [&]() -> double {
    return std::isnan(bound) ? _world->bg()->right() : bound;
  };
  player_r = or_default(phase->player_r(), bound_or_bg_right());
  cam_r = or_default(phase->camera_r(), bound_or_bg_right());
  enemy_r = or_default(phase->enemy_r(), bound_or_bg_right() + 1200.0);
  drink_r = or_default(phase->drink_r(), _world->bg()->right() + 1200.0);

  const double player_jump_to_x = phase->player_jump_to_x();
  const double player_jump_to_z = phase->player_jump_to_z();
  const double player_facing = phase->player_facing();

  std::vector<Value> teams;
  for (Entity* const v : _world->puppet_list()) set_add(teams, Value(v->team()));
  for (Entity* const e : _world->entity_list()) {
    if (!entity::is_fighter(*e) || !set_has(teams, Value(e->team()))) continue;
    if (player_facing == 1.0 || player_facing == -1.0) {
      e->set_facing(Value(player_facing));
    }
    _lfw->mt()->mark = u"criminal_respawn";

    Value x(NullTag{});
    Value z(NullTag{});
    if (!std::isnan(player_jump_to_x)) {
      x = Value(_lfw->mt()->range(lfw::max(player_l, player_jump_to_x - 50.0),
                                  lfw::min(player_r, player_jump_to_x + 50.0)));
    }
    if (!std::isnan(player_jump_to_z)) {
      z = Value(_lfw->mt()->range(lfw::max(_far, player_jump_to_z - 50.0),
                                  lfw::min(_near, player_jump_to_z + 50.0)));
    }
    e->set_position(x, Value(NullTag{}), z);
  }
  const std::vector<DialogInfo>& dialogs = phase->dialogs();
  if (!dialogs.empty()) push_dialogs(dialogs);
}

void Stage::push_dialogs(const std::vector<DialogInfo>& more) {
  const DialogState prev = _dialogs;
  std::vector<DialogInfo> list = prev.list;
  list.insert(list.end(), more.begin(), more.end());
  double index = prev.index;
  if (index < 0.0) {
    index = prev.index + 1.0;
    dialog_time = 0.0;
    const size_t at = static_cast<size_t>(index);
    dialog_end_tester.reset(
        _lfw->end_testers(at < list.size() ? list[at].end_test() : std::vector<std::u16string>()));
  }
  _dialogs = DialogState{index, list};
  _callbacks.call(u"on_dialogs_changed",
                  {StageCallbackArgs{this, dialog_state_value(_dialogs), dialog_state_value(prev)}});
}

void Stage::next_dialog() {
  const DialogState prev = _dialogs;
  if (prev.index >= static_cast<double>(prev.list.size())) return;
  _dialogs = DialogState{prev.index + 1.0, prev.list};
  const size_t at = static_cast<size_t>(_dialogs.index);
  dialog_end_tester.reset(_lfw->end_testers(
      at < _dialogs.list.size() ? _dialogs.list[at].end_test() : std::vector<std::u16string>()));
  dialog_time = 0.0;
  _callbacks.call(u"on_dialogs_changed",
                  {StageCallbackArgs{this, dialog_state_value(_dialogs), dialog_state_value(prev)}});
}

void Stage::clear_dialogs() {
  const DialogState prev = _dialogs;
  _dialogs = DialogState{-1.0, {}};
  _callbacks.call(u"on_dialogs_changed",
                  {StageCallbackArgs{this, dialog_state_value(_dialogs), dialog_state_value(prev)}});
}

void Stage::enter_phase(double idx) {
  if (_world->stage() != this) return;
  _phase_idx = idx;
  const std::vector<StagePhaseInfo>& phases_v = _data.phases();
  const size_t at = static_cast<size_t>(idx);
  const bool in_range = idx >= 0.0 && at < phases_v.size();
  set_phase(in_range ? &phases_v[at] : nullptr);
  _is_stage_finish = !phases_v.empty() && _phase_idx >= static_cast<double>(phases_v.size());
  _is_chapter_finish =
      _is_stage_finish && !strict_equals(field_or(_next_stage, u"chapter"),
                                         _data.chapter().has_value() ? Value(*_data.chapter())
                                                                     : Value());
}

double Stage::ce() {
  double count = 0.0;
  for (Entity* const c : _world->puppet_list()) {
    const Value ce_v = field_or(c->data()->base(), u"ce");
    count += is_nullish(ce_v) ? 1.0 : to_number(ce_v);
  }
  if (!truthy(Value(count))) {
    for (Entity* const e : _world->entity_list()) {
      if (!entity::is_fighter(*e) || !truthy(Value(e->mounted())) || e->hp() <= 0.0) {
        continue;
      }
      if (!strict_equals(Value(e->team()), Value(std::u16string(team_enum::kTeam_1)))) continue;
      const Value ce_v = field_or(e->data()->base(), u"ce");
      count += is_nullish(ce_v) ? 1.0 : to_number(ce_v);
    }
  }
  if (!truthy(Value(count))) count = 1.0;
  const Value difficulty = _world->difficulty();
  if (strict_equals(difficulty, Value(static_cast<double>(Difficulty::Crazy)))) count *= 2.0;
  return count;
}

void Stage::spawn(const StagePhaseInfo& phase, const StageObjectInfo& obj, double count) {
  if (_world->stage() != this) return;
  const double times = std::isnan(obj.times()) ? 1.0 : obj.times();
  double spawn_count =
      std::isnan(obj.ratio()) ? 1.0 : floor(round_float(count * obj.ratio(), 10.0));
  if (spawn_count <= 0.0 || !truthy(Value(times))) return;

  while (spawn_count > 0.0) {
    std::unique_ptr<Item> item = std::make_unique<Item>(this, phase, obj);
    item->spawn();
    items.push_back(std::move(item));
    --spawn_count;
  }
}

void Stage::kill_all() {
  for (const std::unique_ptr<Item>& o : items) {
    for (Entity* const e : o->objects()) {
      if (entity::is_fighter(*e) && strict_equals(Value(e->team()), Value(_team))) e->set_hp(0.0);
    }
  }
}

void Stage::kill_soldiers() {
  for (const std::unique_ptr<Item>& o : items) {
    if (!o->info().is_soldier()) continue;
    for (Entity* const e : o->objects()) {
      if (entity::is_fighter(*e) && strict_equals(Value(e->team()), Value(_team))) e->set_hp(0.0);
    }
  }
}

void Stage::kill_boss() {
  for (const std::unique_ptr<Item>& o : items) {
    if (!o->info().is_boss()) continue;
    for (Entity* const e : o->objects()) {
      if (entity::is_fighter(*e) && strict_equals(Value(e->team()), Value(_team))) e->set_hp(0.0);
    }
  }
}

void Stage::kill_others() {
  for (const std::unique_ptr<Item>& o : items) {
    if (o->info().is_boss() || o->info().is_soldier()) {
      continue;
    }
    for (Entity* const e : o->objects()) {
      if (entity::is_fighter(*e) && strict_equals(Value(e->team()), Value(_team))) e->set_hp(0.0);
    }
  }
}

void Stage::dispose() {
  for (const std::function<void()>& f : _disposers) f();
  for (const std::unique_ptr<Item>& item : items) item->release();

  std::vector<Entity*> temp;
  std::vector<Value> player_teams;
  for (Entity* const v : _world->puppet_list()) set_add(player_teams, Value(v->team()));
  for (Entity* const e : _world->entity_list()) {
    if (entity::is_fighter(*e) && set_has(player_teams, Value(e->team()))) continue;
    if (entity::is_weapon(*e) && e->bearer != nullptr &&
        set_has(player_teams, Value(e->bearer->team()))) {
      continue;
    }
    temp.push_back(e);
  }
  _world->del_entities(temp);
  _callbacks.clear();
}

bool Stage::all_boss_dead() {
  for (const std::unique_ptr<Item>& item : items) {
    if (!item->is_fighter()) continue;
    if (!item->info().is_boss()) continue;
    if (!item->objects().empty()) return false;
    if (!item->released()) return false;
  }
  return true;
}

bool Stage::all_fighter_dead() {
  for (const std::unique_ptr<Item>& item : items) {
    if (!item->is_fighter()) continue;
    if (!item->objects().empty()) return false;
    if (!item->released()) return false;
  }
  return true;
}

bool Stage::dialog_cleared() const {
  return _dialogs.list.size() <= 0 ||
         _dialogs.index >= static_cast<double>(_dialogs.list.size());
}

bool Stage::is_phase_end() {
  if (!phase_end_tester.list().empty()) return phase_end_tester.flow(*this);
  return all_fighter_dead() && dialog_cleared();
}

bool Stage::is_dialog_end() { return dialog_end_tester.flow(*this); }

bool Stage::check_phase_end() {
  const bool ret = is_phase_end();
  if (!ret) return ret;
  enter_phase(phase_idx() + 1.0);
  return ret;
}

bool Stage::check_dialog_end() {
  const bool ret = is_dialog_end();
  if (!ret) return ret;
  next_dialog();
  return ret;
}

bool Stage::should_goto_next_stage() {
  if (is_chapter_finish() || !is_stage_finish()) return false;

  for (Entity* const e : _world->entity_list()) {
    if (!entity::is_fighter(*e)) continue;
    if (e->hp() <= 0.0) continue;
    if (e->position.x >= cam_r) continue;
    const controller::BaseController* const ctrl = e->ctrl();
    if (ctrl != nullptr && ctrl->is_bot()) continue;
    if (ctrl != nullptr && _lfw->players_has(Value(ctrl->player_id))) return false;
  }
  return true;
}

void Stage::update() {
  if (truthy(_phase)) ++phase_time;
  if (dialog() != nullptr) ++dialog_time;
  _fsm.update(1.0);

  const Value* const void_stage = defines::find(u"Defines.VOID_STAGE");
  const Value void_id = void_stage != nullptr ? field_or(*void_stage, u"id") : Value();
  if (equals(Value(id()), void_id)) return;

  _released_items.clear();
  for (const std::unique_ptr<Item>& item : items) {
    if (item->released()) _released_items.push_back(item.get());
    else item->update();
  }
  for (Item* const v : _released_items) {
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].get() == v) {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
        break;
      }
    }
  }

  check_phase_end();
  check_dialog_end();
}


void Stage::assign_number(const std::u16string& key, double v) {
  if (key == u"left") { left = v; return; }
  if (key == u"right") { right = v; return; }
  if (key == u"near") { _near = v; return; }
  if (key == u"far") { _far = v; return; }
  if (key == u"width") { width = v; return; }
  if (key == u"depth") { depth = v; return; }
  if (key == u"player_l") { player_l = v; return; }
  if (key == u"player_r") { player_r = v; return; }
  if (key == u"cam_l") { cam_l = v; return; }
  if (key == u"cam_r") { cam_r = v; return; }
  if (key == u"enemy_l") { enemy_l = v; return; }
  if (key == u"enemy_r") { enemy_r = v; return; }
  if (key == u"drink_l") { drink_l = v; return; }
  if (key == u"drink_r") { drink_r = v; return; }
  if (key == u"phase_time") { phase_time = v; return; }
  if (key == u"dialog_time") { dialog_time = v; return; }
}
}
}
