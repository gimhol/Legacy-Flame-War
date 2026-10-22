#include "lfw/entity/entity.h"

#include "lfw/lfw.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lfw/core/json.h"
#include "lfw/core/js_num.h"
#include "lfw/core/same_ref.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/difficulty.h"
#include "lfw/defines/entity_enum.h"
#include "lfw/defines/entity_group.h"
#include "lfw/defines/facing_flag.h"
#include "lfw/data/next_frame.h"
#include "lfw/defines/frame_id.h"
#include "lfw/defines/game_key.h"
#include "lfw/defines/game_mode.h"
#include "lfw/defines/hit_flag.h"
#include "lfw/entity/entity_flag.h"
#include "lfw/defines/itr_kind.h"
#include "lfw/defines/opoint_kind.h"
#include "lfw/defines/opoint_multi_enum.h"
#include "lfw/defines/opoint_spreading.h"
#include "lfw/defines/speed_ctrl.h"
#include "lfw/defines/speed_mode.h"
#include "lfw/defines/state_enum.h"
#include "lfw/defines/wpoint_kind.h"
#include "lfw/entity/calc_v.h"
#include "lfw/entity/entity_type_check.h"
#include "lfw/entity/entity_snapshot.h"
#include "lfw/entity/face_helper.h"
#include "lfw/entity/summary_mgr.h"
#include "lfw/factory.h"
#include "lfw/ground.h"
#include "lfw/loader/get_val_from_entity.h"
#include "lfw/loader/preprocess_next_frame.h"
#include "lfw/state/entity_states.h"
#include "lfw/state/state_base.h"
#include "lfw/world.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/container_help/find.h"
#include "lfw/utils/cross_bounding.h"
#include "lfw/utils/math/base.h"
#include "lfw/utils/math/clamp.h"
#include "lfw/utils/math/clamp_add.h"
#include "lfw/utils/math/float_equal.h"
#include "lfw/utils/math/round_float.h"
#include "lfw/utils/type_check.h"

#include <cmath>
#include <limits>

namespace lfw {
namespace {

bool is_integer(double x) { return std::isfinite(x) && std::floor(x) == x; }

double num_destructured(const Value& v, double fallback) {
  return std::holds_alternative<std::monostate>(v) ? fallback : to_number(v);
}

bool nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

Value or_nullish(const Value& v, const Value& fallback) {
  return nullish(v) ? fallback : v;
}

double num_of(const Value& v) { return nullish(v) ? 0.0 : to_number(v); }

std::optional<double> opt_num(const Value& v) {
  return nullish(v) ? std::nullopt : std::optional<double>(to_number(v));
}

std::u16string frame_id_of(const Entity& e) { return to_string(e.frame.id()); }

std::u16string frame_id_value(const Value& f) { return to_string(field_or(f, u"id")); }

std::u16string frame_id_value(const FrameInfo& f) { return f.id(); }

Value or_nan(const std::optional<double>& v) {
  return v.has_value() ? Value(*v) : Value(std::numeric_limits<double>::quiet_NaN());
}

Value& tick_slot(std::vector<Value>& nums, entity::NSlot slot, std::size_t k) {
  return nums[static_cast<std::size_t>(slot) + k];
}

void write_tick(const Times& t, std::vector<Value>& nums, entity::NSlot slot) {
  std::vector<double> buf(5, 0.0);
  t.write_nums(buf, 0);
  for (std::size_t k = 0; k < 5; ++k) tick_slot(nums, slot, k) = Value(buf[k]);
}

void read_tick(Times& t, const std::vector<Value>& nums, entity::NSlot slot) {
  std::vector<double> buf(5, 0.0);
  for (std::size_t k = 0; k < 5; ++k) {
    buf[k] = to_number(nums[static_cast<std::size_t>(slot) + k]);
  }
  t.read_nums(buf, 0);
}

bool not_zero(const Value& v) { return !strict_equals(v, Value(0.0)); }

Value next_frame_of(const Value& a, const Value& b) { return or_nullish(a, b); }

std::size_t array_length(const Value& v) {
  const Array* a = as_array(v);
  return a != nullptr ? a->size() : 0;
}

Value position_value(const Vector3& p);

}

Entity::Entity(World& world, const EntityData& data)
    : Entity(world, data, &state::entity_states()) {}

Entity::Entity(World& world, const EntityData& data, state::States* states) {
  world_ = &world;
  _data = data;
  states_ = states;
  _atom_time = world.dataset.atom_time();
  terrain = Ground::horizon();
  {
    Object nf;
    nf.set(u"id", Value(std::u16string()));
    _next_frame_by_id = Value(std::make_shared<Object>(nf));
  }
  reset(_data, states);
}

LFW& Entity::lfw() const { return world_->lfw(); }

Entity::~Entity() = default;

void Entity::reset(const EntityData& data) { reset(data, &state::entity_states()); }

void Entity::reset(const EntityData& data, state::States* states) {
  marks.clear();
  buffs.clear();
  is_on_ground = false;
  terrain = Ground::horizon();
  _data = data;
  _origin_data_id = _data.id();
  id = world().lfw().new_id();
  wait = 0;
  _motionless_ticks = 0;
  _lifetime = 0;
  fallinjury = 0;
  _ground_y = 0;
  variant = 0;
  transforms.clear();
  transform_index = 0;
  _reserve = 0;
  _mounted = 0;
  _ghosted = 0;
  prev_position.set(kMinSafeInteger, kMinSafeInteger, kMinSafeInteger);
  position.set(0, 0, 0);
  fuse_bys.clear();
  has_fuse_bys = false;
  dismiss_time = std::nullopt;
  dismiss_data.reset();
  copies.clear();
  stat_bar = 0;
  _toughness_resting_max = defines::num(u"Defines.DEFAULT_TOUGHNESS_RESTING_MAX");
  _resting_max = opt_num(field_or(_data.base(), u"resting_max"));
  _resting = 0;
  _toughness = 0;
  _toughness_max = 0;
  _toughness_resting = 0;
  _fall_value_max = opt_num(field_or(_data.base(), u"fall_value_max"));
  _defend_value_max = opt_num(field_or(_data.base(), u"defend_value_max"));
  _defend_ratio = opt_num(field_or(_data.base(), u"defend_ratio"));
  _catch_time_max = opt_num(field_or(_data.base(), u"catch_time_max"));
  throwinjury = 0;
  facing = 1;
  frame = empty_frame_info();
  _prev_frame = frame;
  set_catching(nullptr);
  catcher = nullptr;
  wakeup_invuln = 0;
  name_visible = 0;
  _outline_alpha = 0.8;
  velocity.set(0, 0, 0);
  prev_velocity.set(0, 0, 0);
  callbacks.clear();
  _name = Value(NullTag{});
  _team = world().lfw().new_team();
  _landing_frame = FrameInfo();
  bearer = nullptr;
  holding = nullptr;
  emitters.clear();
  _arest = 0;
  vrests.clear();
  blockers.clear();
  superpunchs.clear();
  motionless = 0;
  shaking = 0;
  bounced = false;
  lying_a_count = Value(0.0);
  lying_d_count = Value(0.0);
  lying_c_count = Value(0.0);
  drop_hurted = false;
  dropping = false;
  states_ = states;
  _hp_r_tick.set_max(num_of(dataset(u"hp_r_ticks")));
  _hp_r_tick.set_value(0);
  _mp_r_tick.set_max(num_of(dataset(u"mp_r_ticks")));
  _mp_r_tick.set_value(0);
  _fall_r_tick.set_max(num_of(dataset(u"fall_r_ticks")));
  _fall_r_tick.set_value(0);
  _defend_r_tick.set_max(num_of(dataset(u"defend_r_ticks")));
  _defend_r_tick.set_value(0);
  _toughness_r_value = num_of(dataset(u"toughness_r_value"));
  _defend_r_value = num_of(dataset(u"defend_r_value"));
  _fall_r_value = num_of(dataset(u"fall_r_value"));
  _hp_max = opt_num(dataset(u"hp_max"));
  _mp_max = opt_num(dataset(u"mp_max"));
  _defend_ratio = opt_num(field_or(_data.base(), u"defend_ratio"));
  jumping.x = 0;
  jumping.y = 0;
  jumping.z = 0;
  jumping.t = 0;
  if (ctrl_ != nullptr) world().lfw().release_ctrl(ctrl_);
  ctrl_ = world().lfw().acquire_invalid_ctrl(world());
  reset_armor();
  set_fall_value(fall_value_max());
  set_defend_value(defend_value_max());
  _hp_r = hp_max();
  _hp = _hp_r;
  _mp = mp_max();
  set_catch_time(catch_time_max());
  _invisible = 0;
  _invulnerable = 0;
  _blinking = 0;
  _after_blink = std::nullopt;
  _state = nullptr;
  dead_gone = 0;
  dead_join = Value(NullTag{});
  ctrl_visible = 0;
  const Value drink_info = field_or(_data.base(), u"drink");
  drink = truthy(drink_info) ? std::make_unique<DrinkInfo>(drink_info) : nullptr;
  opoints.clear();
  _prev_cpoint_a = Value(NullTag{});
  collision_list.clear();
  collided_list.clear();
  lastest_collided = std::nullopt;
  _outline_color.clear();
  _outline_alpha = 0.8;
  _outline_width = 1;
  _outline_enabled = std::nullopt;
  _mix_color.clear();
  _mix_strength = 0;
  _greyscale = 0;
  _render_effect_time = 0;
  auto_key_role();
}

void Entity::set_state(const Value& state_code) {
  state::State_Base* v = states_->get(state_code);
  if (v == nullptr) v = &states_->fallback(Value(_data.type()), state_code);
  if (_state == v) return;
  if (_state != nullptr) _state->leave(*this, frame);
  _state = v;
  if (_state != nullptr && _state->enter) _state->enter(*this, get_prev_frame());
}

std::u16string Entity::outline_color() const {
  if (!_outline_color.empty()) return _outline_color;
  const Value* map = defines::find(u"Defines.TeamInfoMap");
  const Value info = map != nullptr ? field_or(*map, _team.c_str()) : Value();
  const Value c = field_or(info, u"outline_color");
  return truthy(c) ? to_string(c) : std::u16string();
}

void Entity::set_outline_color(std::u16string v) {
  _outline_color = std::move(v);
  ++_render_effect_time;
}

void Entity::set_outline_alpha(double v) {
  _outline_alpha = v;
  ++_render_effect_time;
}

void Entity::set_outline_width(double v) {
  _outline_width = v;
  ++_render_effect_time;
}

Value Entity::outline_enabled() const {
  return _outline_enabled.has_value() ? Value(*_outline_enabled) : dataset(u"outline_enabled");
}

void Entity::set_outline_enabled(const Value& v) {
  _outline_enabled = opt_num(v);
  ++_render_effect_time;
}

void Entity::set_mix_color(std::u16string v) {
  _mix_color = std::move(v);
  ++_render_effect_time;
}

void Entity::set_mix_strength(double v) {
  _mix_strength = v;
  ++_render_effect_time;
}

void Entity::set_greyscale(double v) {
  _greyscale = v;
  ++_render_effect_time;
}

std::u16string Entity::origin_data_id() const {
  if (!_origin_data_id.empty()) return _origin_data_id;
  return _data.id();
}

Value Entity::group() const { return field_or(_data.base(), u"group"); }

void Entity::set_reserve(double v) {
  v = round_float(v);
  const double o = _reserve;
  if (o == v) return;
  _reserve = v;
  callbacks.call(u"on_reserve_changed", {Value(v), Value(o)});
}

double Entity::type() const { return _data.type(); }

const std::vector<ItrInfo>& Entity::itr() const { return frame.itr(); }

const std::vector<BdyInfo>& Entity::bdy() const { return frame.bdy(); }

void Entity::set_toughness_resting_max(double v) { _toughness_resting_max = round_float(v); }

double Entity::resting_max() const {
  return _resting_max.has_value() ? *_resting_max : world().dataset.resting_max();
}

void Entity::set_resting_max(double v) { _resting_max = round_float(v); }

void Entity::set_resting(double v) { _resting = round_float(v); }

void Entity::set_fall_value(double v) {
  const double o = _fall_value;
  if (o == v) return;
  _fall_value = round_float(v);
  if (v < o) {
    set_resting(resting_max());
    set_toughness_resting(toughness_resting_max());
  }
}

void Entity::set_toughness(double v) {
  v = round_float(v);
  if (v < 0) v = 0;
  const double o = _toughness;
  if (o == v) return;
  _toughness = v;
  if (v < o) set_toughness_resting(toughness_resting_max());
}

void Entity::set_toughness_max(double v) { _toughness_max = round_float(v); }

void Entity::set_toughness_resting(double v) {
  v = round_float(v);
  const double o = _toughness_resting;
  if (o == v) return;
  _toughness_resting = v;
}

double Entity::catch_time_max() const {
  return _catch_time_max.has_value() ? *_catch_time_max
                                     : world().dataset.catch_time_max();
}

void Entity::set_catch_time_max(double v) { _catch_time_max = round_float(v); }

double Entity::fall_value_max() const {
  return _fall_value_max.has_value() ? *_fall_value_max
                                     : world().dataset.fall_value_max();
}

void Entity::set_fall_value_max(double v) { _fall_value_max = round_float(v); }

void Entity::set_defend_value(double v) {
  const double o = _defend_value;
  if (o == v) return;
  _defend_value = round_float(v);
  if (v < o) {
    set_resting(resting_max());
    set_toughness_resting(toughness_resting_max());
  }
}

double Entity::defend_value_max() const {
  return _defend_value_max.has_value() ? *_defend_value_max
                                       : world().dataset.defend_value_max();
}

void Entity::set_defend_value_max(double v) { _defend_value_max = round_float(v); }

double Entity::defend_ratio() const {
  return _defend_ratio.has_value() ? *_defend_ratio
                                   : world().dataset.defend_ratio();
}

void Entity::set_defend_ratio(double v) { _defend_ratio = round_float(v); }

Value Entity::name() const {
  if (!std::holds_alternative<NullTag>(_name)) return _name;
  if (ctrl_ != nullptr && ctrl_->is_human()) {
    const Value pname = field_or(ctrl_->player, u"name");
    if (truthy(pname)) return pname;
    return Value(u"Player " + to_string(field_or(ctrl_->player, u"id")));
  }
  const Value base_name = field_or(_data.base(), u"name");
  return nullish(base_name) ? Value(std::u16string()) : base_name;
}

void Entity::set_name(const Value& v) {
  if (strict_equals(v, name())) return;
  const Value o = _name;
  _name = v;
  callbacks.call(u"on_name_changed", {truthy(v) ? v : Value(std::u16string()), o});
}

void Entity::set_mp(double v) {
  const double o = _mp;
  v = max(0.0, v);
  v = round_float(v);
  if (o == v) return;
  _mp = v;
  if (v < o) {
    const std::shared_ptr<Summary> s = summary_mgr().get(id);
    s->set_mp_usage(Value(to_number(s->mp_usage()) + (o - v)));
  }
  if (v < o && !defines::is_independent(_team)) {
    const std::shared_ptr<Summary> s = summary_mgr().get(_team);
    s->set_mp_usage(Value(to_number(s->mp_usage()) + (o - v)));
  }
  callbacks.call(u"on_mp_changed", {Value(v), Value(o)});
  if (o > 0 && v <= 0) {
    const Value nf = next_frame_of(frame.on_exhaustion().to_value(),
                                   _data.on_exhaustion().to_value());
    if (truthy(nf)) enter_frame(nf);
  }
}

void Entity::set_hp_r(double v) { _hp_r = round_float(max(0.0, v)); }

void Entity::set_hp(double v) {
  const double o = _hp;
  v = max(0.0, v);
  v = round_float(v);
  if (o == v) return;
  _hp = v;
  if (v < o) {
    const std::shared_ptr<Summary> s = summary_mgr().get(id);
    s->set_hp_lost(Value(to_number(s->hp_lost()) + (o - v)));
  }
  if (v < o && !defines::is_independent(_team)) {
    const std::shared_ptr<Summary> s = summary_mgr().get(_team);
    s->set_hp_lost(Value(to_number(s->hp_lost()) + (o - v)));
  }
  callbacks.call(u"on_hp_changed", {Value(v), Value(o)});
  if (ctrl_ != nullptr && ctrl_->is_human() && ((o > 0) != (v > 0))) {
    world().mark_players_alive(*this, v > 0);
  }
  if (o > 0 && v <= 0) {
    callbacks.call(u"on_dead", {});
    if (_state != nullptr && _state->on_dead) _state->on_dead(*this);
    const Value brokens = field_or(_data.base(), u"brokens");
    if (!strict_equals(state(), Value(static_cast<double>(StateEnum::Gone))) &&
        frame_id_of(*this) != std::u16string(frame_id::kGone) &&
        array_length(brokens) > 0) {
      apply_opoints(brokens);
      play_sound(field_or(_data.base(), u"dead_sounds"));
    }
    const Value nf = next_frame_of(frame.on_dead().to_value(), _data.on_dead().to_value());
    if (truthy(nf)) enter_frame(nf);
  }
  if (v > _hp_r) set_hp_r(v);
}

double Entity::mp_max() const {
  return _mp_max.has_value() ? *_mp_max : world().dataset.mp_max();
}

void Entity::set_mp_max(double v) { _mp_max = round_float(max(0.0, v)); }

double Entity::hp_max() const {
  return _hp_max.has_value() ? *_hp_max : world().dataset.hp_max();
}

void Entity::set_hp_max(double v) { _hp_max = round_float(max(0.0, v)); }

void Entity::set_team(std::u16string v) {
  if (equals(Value(v), Value(_team))) return;
  const std::u16string o = _team;
  _team = std::move(v);
  const double n = to_number(Value(_team));
  variant = truthy(Value(n)) ? n : 0;
  callbacks.call(u"on_team_changed", {Value(_team), Value(o)});
  ++_render_effect_time;
}

const std::u16string* Entity::src_emitter() const {
  return emitters.empty() ? nullptr : &emitters.front();
}

const std::u16string* Entity::emitter() const {
  return emitters.empty() ? nullptr : &emitters.back();
}

void Entity::set_blinking(double v) { _blinking = round_float(max(0.0, v)); }

void Entity::set_invisible(double v) { _invisible = round_float(max(0.0, v)); }

void Entity::set_invulnerable(double v) { _invulnerable = round_float(max(0.0, v)); }

Value Entity::bot_ignore() const {
  return or_nullish(frame.has(u"bot_ignore") ? Value(frame.bot_ignore()) : Value(),
                    field_or(_data.base(), u"bot_ignore"));
}

void Entity::set_ctrl(controller::BaseController* v) {
  if (v == nullptr) return;
  if (ctrl_ == v) return;
  controller::BaseController* prev = ctrl_;
  ctrl_ = v;
  callbacks.call(u"on_ctrl_changed", {});
  world().mark_players_alive(*this, ctrl_->is_human() && hp() > 0);
  if (prev != nullptr) world().lfw().release_ctrl(prev);
  sync_ctrl();
}

void Entity::as_key_role(const Value& v) {
  name_visible = truthy(v) ? 1 : 0;
  wakeup_invuln = truthy(v) ? 1 : 0;
  dead_gone = truthy(v) ? 0 : 1;
}

void Entity::auto_key_role() {
  bool v = false;
  const Array* group_arr = as_array(group());
  if (group_arr != nullptr) {
    for (std::size_t i = 0; i < group_arr->size(); ++i) {
      const Value& item = group_arr->at(i);
      if (equals(item, Value(std::u16string(entity_group::kRegular))) ||
          equals(item, Value(std::u16string(entity_group::kBoss)))) {
        v = true;
        break;
      }
    }
  }
  as_key_role(Value(v));
}

double Entity::gravity() const {
  const Value g1 = _state != nullptr && _state->get_gravity ? _state->get_gravity(mutable_self())
                                                            : Value();
  const Value g2 = ctrl_ != nullptr && ctrl_->is_end(gk::kDefend) ? dataset(u"gravity")
                                                                  : dataset(u"gravity_d");
  return nullish(g1) ? num_of(g2) : to_number(g1);
}

double Entity::itr_motionless() const {
  if (type() == static_cast<double>(EntityEnum::Ball)) {
    return num_of(dataset(u"ball_itr_motionless"));
  }
  return num_of(dataset(u"itr_motionless"));
}


const FrameInfo* Entity::find_frame_by_id(const Value& id_value) const {
  if (_state != nullptr && _state->find_frame_by_id) {
    const FrameInfo* const r = _state->find_frame_by_id(mutable_self(), id_value);
    if (r != nullptr) return r;
  }
  if (std::holds_alternative<std::monostate>(id_value)) return &frame;
  const std::u16string* s = std::get_if<std::u16string>(&id_value);
  if (s != nullptr) {
    if (*s == frame_id::kNone || *s == frame_id::kSelf) return &frame;
    if (*s == frame_id::kAuto) return find_auto_frame();
    if (*s == frame_id::kGone) return &gone_frame_info();
  }
  const FrameInfo* const found = data_frame(to_string(id_value));
  if (found == nullptr || found->is_nullish()) return find_auto_frame();
  return found;
}

const FrameInfo* Entity::find_auto_frame() const {
  if (_state != nullptr && _state->get_auto_frame) {
    const FrameInfo* const f = _state->get_auto_frame(mutable_self());
    if (f != nullptr) return f;
  }
  const FrameInfo* const f0 = data_frame(u"0");
  if (f0 != nullptr && !f0->is_nullish()) return f0;
  return &frame;
}

Value Entity::find_align_frame(const std::u16string& frame_id, const Value& src,
                               const Value& dst) const {
  const Array* d = as_array(dst);
  const Array* s = as_array(src);
  const std::size_t d_len = d != nullptr ? d->size() : 0;
  const std::size_t s_len = s != nullptr ? s->size() : 0;
  if (d_len > 0 && s_len > 0) {
    std::size_t idx = s_len;
    for (std::size_t i = 0; i < s_len; ++i) {
      if (strict_equals(s->at(i), Value(frame_id))) {
        idx = i;
        break;
      }
    }
    Object nf;
    nf.set(u"id", d->at((idx + 1) % d_len));
    return Value(std::make_shared<Object>(nf));
  }
  if (d_len > 0) {
    Object nf;
    nf.set(u"id", d->at(0));
    return Value(std::make_shared<Object>(nf));
  }
  // 兜底交给 auto 帧：整帧对象本身也是合法的 next-frame 描述符
  return find_auto_frame()->to_value();
}

Value Entity::get_sudden_death_frame() const {
  if (_state != nullptr && _state->get_sudden_death_frame) {
    const Value v = _state->get_sudden_death_frame(mutable_self());
    if (truthy(v)) return v;
  }
  const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
  return auto_frame != nullptr ? *auto_frame : Value();
}

Value Entity::get_caught_end_frame() {
  if (position.y < _ground_y) position.y = _ground_y + 1;
  if (_state != nullptr && _state->get_caught_end_frame) {
    const Value v = _state->get_caught_end_frame(mutable_self());
    if (truthy(v)) return v;
  }
  const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
  return auto_frame != nullptr ? *auto_frame : Value();
}

double Entity::handle_facing_flag(const Value& facing_value) const {
  const double* d = std::get_if<double>(&facing_value);
  if (d == nullptr) return this->facing;
  const double cur = this->facing;
  const int lr = ctrl_ != nullptr ? ctrl_->LR() : 0;
  const Value catcher_facing =
      catcher != nullptr ? Value(catcher->facing) : Value();
  const Value bearer_facing = bearer != nullptr ? Value(bearer->facing) : Value();
  if (*d == static_cast<double>(FacingFlag::Ctrl))
    return lr != 0 ? static_cast<double>(lr) : cur;
  if (*d == static_cast<double>(FacingFlag::AntiCtrl))
    return lr != 0 ? to_number(entity::turn_face(Value(static_cast<double>(lr)))) : cur;
  if (*d == static_cast<double>(FacingFlag::SameAsCatcher))
    return truthy(catcher_facing) ? to_number(catcher_facing) : cur;
  if (*d == static_cast<double>(FacingFlag::OpposingCatcher))
    return truthy(entity::turn_face(catcher_facing))
               ? to_number(entity::turn_face(catcher_facing))
               : cur;
  if (*d == static_cast<double>(FacingFlag::Backward))
    return to_number(entity::turn_face(Value(cur)));
  if (*d == static_cast<double>(FacingFlag::Left) ||
      *d == static_cast<double>(FacingFlag::Right))
    return *d;
  if (*d == static_cast<double>(FacingFlag::VX))
    return velocity.x > 0 ? 1 : velocity.x < 0 ? -1 : cur;
  if (*d == static_cast<double>(FacingFlag::AntiVX))
    return velocity.x > 0 ? -1 : velocity.x < 0 ? 1 : cur;
  if (*d == static_cast<double>(FacingFlag::Trend)) {
    if (lr != 0) return static_cast<double>(lr);
    return velocity.x > 0 ? 1 : velocity.x < 0 ? -1 : cur;
  }
  if (*d == static_cast<double>(FacingFlag::SameAsBearer))
    return truthy(bearer_facing) ? to_number(bearer_facing) : cur;
  if (*d == static_cast<double>(FacingFlag::OpposingBearer))
    return truthy(entity::turn_face(bearer_facing))
               ? to_number(entity::turn_face(bearer_facing))
               : cur;
  return cur;
}

double Entity::handle_wait_flag(const Value& wait_value, const FrameInfo* frame_value) const {
  const bool has_frame = frame_value != nullptr;
  if (nullish(wait_value) && has_frame) return get_frame_wait(*frame_value);
  if (is_positive(wait_value)) return to_number(wait_value);
  const std::u16string* s = std::get_if<std::u16string>(&wait_value);
  if ((s != nullptr && *s == u"i") || !has_frame) return this->wait;
  if (s != nullptr && *s == u"d") {
    return max(0.0, frame_value->wait() - to_number(this->frame.wait()) + this->wait);
  }
  return get_frame_wait(*frame_value);
}

double Entity::get_frame_wait(const FrameInfo& frame_value) const {
  const double d = frame_value.wait() + world().dataset.wait_offset();
  return _from_wait_block ? d - _atom_time : d;
}

Value Entity::dvx() const {
  if (!frame.has(u"dvx")) return Value();
  const double v = frame.dvx();
  if (!truthy(Value(v))) return Value(v);
  return Value(v * to_number(dataset(u"fvx_f")));
}

Value Entity::dvy() const {
  if (!frame.has(u"dvy")) return Value();
  const double v = frame.dvy();
  if (!truthy(Value(v))) return Value(v);
  return Value(v * to_number(dataset(u"fvy_f")));
}

Value Entity::dvz() const {
  if (!frame.has(u"dvz")) return Value();
  const double v = frame.dvz();
  if (!truthy(Value(v))) return Value(v);
  return Value(v * to_number(dataset(u"fvz_f")));
}

void Entity::set_velocity(const Value& x, const Value& y, const Value& z) {
  if (!nullish(x)) prev_velocity.x = velocity.x = round_float(to_number(x));
  if (!nullish(y)) prev_velocity.y = velocity.y = round_float(to_number(y));
  if (!nullish(z)) prev_velocity.z = velocity.z = round_float(to_number(z));
  if (velocity.y > 0) leave_ground();
}

void Entity::leave_ground() {
  if (eqlt(position.y, _ground_y)) position.y = round_float(_ground_y + 0.1);
  is_on_ground = false;
}

void Entity::handle_ground_velocity_decay(double factor) {
  if (position.y > _ground_y || truthy(Value(shaking)) || truthy(Value(motionless))) return;
  const bool landing = _landing_frame.same_as(frame);
  factor *= to_number(dataset(landing ? u"land_friction_factor" : u"friction_factor"));
  const Value fx = dataset(landing ? u"land_friction_x" : u"friction_x");
  const Value fz = dataset(landing ? u"land_friction_z" : u"friction_z");
  handle_velocity_decay(fx,
                        std::holds_alternative<std::monostate>(fz)
                            ? std::nullopt
                            : std::optional<Value>(fz),
                        factor);
}

void Entity::handle_velocity_decay(const Value& accx, std::optional<Value> accz,
                                   double factor) {
  const double atom_time = world().dataset.atom_time();
  Value x(round_float(to_number(velocity.x) * std::pow(factor, atom_time)));
  Value z(round_float(to_number(velocity.z) * std::pow(factor, atom_time)));
  const Value accz_value = accz.has_value() ? *accz : accx;
  const double acc_x = round_float(to_number(accx) * atom_time);
  const double acc_z = round_float(to_number(accz_value) * atom_time);

  const Value ctrl_x = frame.ctrl_x();
  const Value ctrl_z = frame.ctrl_z();
  Value dvx_value = dvx();
  if (std::holds_alternative<std::monostate>(dvx_value)) dvx_value = Value(0.0);
  Value dvz_value = dvz();
  if (std::holds_alternative<std::monostate>(dvz_value)) dvz_value = Value(0.0);
  const int lr = ctrl_ != nullptr ? ctrl_->LR() : 0;
  const int ud = ctrl_ != nullptr ? ctrl_->UD() : 0;
  if (truthy(ctrl_x) && lr == 0) dvx_value = Value(0.0);
  if (truthy(ctrl_z) && ud == 0) dvz_value = Value(0.0);

  const double dvx_num = to_number(dvx_value);
  const double dvz_num = to_number(dvz_value);
  if (gt(x, dvx_value)) {
    x = Value(to_number(x) - acc_x);
    if (lt(x, dvx_value)) x = dvx_value;
  } else if (lt(x, Value(-dvx_num))) {
    x = Value(to_number(x) + acc_x);
    if (gt(x, Value(-dvx_num))) x = Value(-dvx_num);
  }
  if (gt(z, dvz_value)) {
    z = Value(to_number(z) - acc_z);
    if (lt(z, dvz_value)) z = dvz_value;
  } else if (lt(z, Value(-dvz_num))) {
    z = Value(to_number(z) + acc_z);
    if (gt(z, Value(-dvz_num))) z = Value(-dvz_num);
  }
  set_velocity(x, Value(NullTag{}), z);
}

void Entity::handle_gravity() {
  if (bearer != nullptr || catcher != nullptr || truthy(Value(shaking)) ||
      truthy(Value(motionless)))
    return;
  const bool enabled = frame.gravity_enabled().value_or(true);
  if (position.y <= _ground_y || !enabled) return;
  velocity.y = round_float(velocity.y - gravity() * _atom_time);
}

void Entity::update_velocity(const FrameInfo& vinfo) {
  if (bearer != nullptr || catcher != nullptr || truthy(Value(shaking)) ||
      truthy(Value(motionless)))
    return;
  const double atom_time = _atom_time;

  Value dvx = Value(vinfo.dvx());
  Value dvy = Value(vinfo.dvy());
  Value dvz = Value(vinfo.dvz());
  if (truthy(dvx)) dvx = Value(round_float(to_number(dvx) * to_number(dataset(u"fvx_f"))));
  if (truthy(dvy)) dvy = Value(round_float(to_number(dvy) * to_number(dataset(u"fvy_f"))));
  if (truthy(dvz)) dvz = Value(round_float(to_number(dvz) * to_number(dataset(u"fvz_f"))));

  Value vxm = std::isnan(vinfo.vxm()) ? Value(static_cast<double>(SpeedMode::Default))
                                      : Value(vinfo.vxm());
  Value vym = std::isnan(vinfo.vym()) ? Value(static_cast<double>(SpeedMode::AccTo))
                                      : Value(vinfo.vym());
  Value vzm = std::isnan(vinfo.vzm()) ? Value(static_cast<double>(SpeedMode::Default))
                                      : Value(vinfo.vzm());
  Value ctrl_x = std::isnan(vinfo.ctrl_x()) ? Value(0.0) : Value(vinfo.ctrl_x());
  Value ctrl_y = std::isnan(vinfo.ctrl_y()) ? Value(0.0) : Value(vinfo.ctrl_y());
  Value ctrl_z = std::isnan(vinfo.ctrl_z()) ? Value(0.0) : Value(vinfo.ctrl_z());

  Value acc_x = Value(vinfo.acc_x());
  Value acc_y = Value(vinfo.acc_y());
  Value acc_z = Value(vinfo.acc_z());
  if ((equals(vxm, Value(static_cast<double>(SpeedMode::AccTo))) ||
       equals(vxm, Value(static_cast<double>(SpeedMode::FixedAccTo)))) &&
      std::isnan(vinfo.acc_x()) && truthy(dvx))
    acc_x = dvx;
  if ((equals(vym, Value(static_cast<double>(SpeedMode::AccTo))) ||
       equals(vym, Value(static_cast<double>(SpeedMode::FixedAccTo)))) &&
      std::isnan(vinfo.acc_y()) && truthy(dvy))
    acc_y = dvy;
  if ((equals(vzm, Value(static_cast<double>(SpeedMode::AccTo))) ||
       equals(vzm, Value(static_cast<double>(SpeedMode::FixedAccTo)))) &&
      std::isnan(vinfo.acc_z()) && truthy(dvz))
    acc_z = dvz;
  if (truthy(acc_x)) acc_x = Value(round_float(to_number(acc_x) * atom_time));
  if (truthy(acc_y)) acc_y = Value(round_float(to_number(acc_y) * atom_time));
  if (truthy(acc_z)) acc_z = Value(round_float(to_number(acc_z) * atom_time));

  double vx = velocity.x;
  double vy = velocity.y;
  double vz = velocity.z;
  const int lr = ctrl_ != nullptr ? ctrl_->LR() : 0;
  const int ud = ctrl_ != nullptr ? ctrl_->UD() : 0;
  const int jd = ctrl_ != nullptr ? ctrl_->jd() : 0;
  const Value facing_v = Value(facing);
  const auto apply = [](double current, const Value& value, const Value& mode,
                        const Value& acc, const Value& direction) {
    return entity::calc_v(current, to_number(value), mode, acc, direction);
  };

  if (nullish(dvx)) {
  } else if (!truthy(ctrl_x)) {
    vx = apply(vx, dvx, vxm, acc_x, facing_v);
  } else if (lr != 0 && equals(ctrl_x, Value(static_cast<double>(SpeedCtrl::Control)))) {
    vx = apply(vx, dvx, vxm, acc_x, Value(static_cast<double>(lr)));
  } else if (lr != 0 && equals(ctrl_x, Value(static_cast<double>(SpeedCtrl::Enable)))) {
    vx = apply(vx, dvx, vxm, acc_x, Value(1.0));
  } else if (lr == 0 && equals(ctrl_x, Value(static_cast<double>(SpeedCtrl::Disable)))) {
    vx = apply(vx, dvx, vxm, acc_x, Value(1.0));
  }

  if (nullish(dvy)) {
  } else if (!truthy(ctrl_y)) {
    vy = apply(vy, dvy, vym, acc_y, Value(1.0));
  } else if (jd != 0 && equals(ctrl_y, Value(static_cast<double>(SpeedCtrl::Control)))) {
    vy = apply(vy, dvy, vym, acc_y, Value(static_cast<double>(jd)));
  } else if (jd != 0 && equals(ctrl_y, Value(static_cast<double>(SpeedCtrl::Enable)))) {
    vy = apply(vy, dvy, vym, acc_y, Value(1.0));
  } else if (jd == 0 && equals(ctrl_y, Value(static_cast<double>(SpeedCtrl::Disable)))) {
    vy = apply(vy, dvy, vym, acc_y, Value(1.0));
  }

  if (nullish(dvz)) {
  } else if (!truthy(ctrl_z)) {
    vz = apply(vz, dvz, vzm, acc_z, Value(1.0));
  } else if (ud != 0 && equals(ctrl_z, Value(static_cast<double>(SpeedCtrl::Control)))) {
    vz = apply(vz, dvz, vzm, acc_z, Value(static_cast<double>(ud)));
  } else if (ud != 0 && equals(ctrl_z, Value(static_cast<double>(SpeedCtrl::Enable)))) {
    vz = apply(vz, dvz, vzm, acc_z, Value(1.0));
  } else if (ud == 0 && equals(ctrl_z, Value(static_cast<double>(SpeedCtrl::Disable)))) {
    vz = apply(vz, dvz, vzm, acc_z, Value(1.0));
  }

  velocity.x = round_float(vx);
  velocity.y = round_float(vy);
  velocity.z = round_float(vz);
}

void Entity::set_arest(double v) {
  if (equals(Value(v), Value(_arest))) return;
  _arest = round_float(v);
}

double Entity::weight() const { return num_of(or_nullish(field_or(_data.base(), u"weight"), Value(1.0))); }

double Entity::base_type() const { return _data.base_type(); }

Value Entity::state() const { return frame.valid() ? Value(frame.state()) : Value(); }

void Entity::reset_armor() {
  armor = ArmorInfo(field_or(_data.base(), u"armor"));
  const double toughness = armor.toughness();
  set_toughness_max(num_of(toughness));
  set_toughness(num_of(toughness));
  set_toughness_resting(0);
  set_toughness_resting_max(num_of(armor.toughness_resting()));
  _toughness_r_value = std::isnan(armor.toughness_r_value())
                           ? num_of(dataset(u"toughness_r_value"))
                           : armor.toughness_r_value();
  _toughness_r_tick.set_max(std::isnan(armor.toughness_r_tick())
                                ? num_of(dataset(u"toughness_r_tick"))
                                : armor.toughness_r_tick());
  _toughness_r_tick.set_value(0);
}

Entity& Entity::set_catching(Entity* v) {
  if (catching == v) return *this;
  catching = v;
  return *this;
}

Entity& Entity::add_catch_time(double value) {
  if (!truthy(Value(value))) return *this;
  return set_catch_time(_catch_time + value);
}

Entity& Entity::set_catch_time(double value) {
  const double v = round_float(value);
  if (equals(Value(_catch_time), Value(v))) return *this;
  _catch_time = clamp(v, 0.0, catch_time_max());
  return *this;
}

Value Entity::dataset(const std::u16string& name) const {
  Value v = frame.dataset().get(name);
  if (nullish(v)) v = field_or(_data.base(), name.c_str());
  if (nullish(v)) v = world().bg_dataset(name);
  if (nullish(v)) v = world().dataset.get(name);
  return v;
}


Value Entity::data_base_hit_sounds() const { return field_or(_data.base(), u"hit_sounds"); }

Value Entity::data_id() const { return Value(_data.id()); }

Value Entity::frame_behavior() const { return frame.behavior(); }

bool Entity::has_data_indexes() const { return _data.has_field(u"indexes"); }

double Entity::ctrl_ud() const {
  return ctrl_ != nullptr ? static_cast<double>(ctrl_->UD()) : 0.0;
}

double Entity::ctrl_lr() const {
  return ctrl_ != nullptr ? static_cast<double>(ctrl_->LR()) : 0.0;
}

bool Entity::ctrl_is_bot() const { return entity::is_bot_ctrl(ctrl_); }

bool Entity::ctrl_is_end(const std::u16string& key) const {
  return ctrl_ != nullptr ? ctrl_->is_end(key) : true;
}

void Entity::ctrl_reset_key_list() {
  if (ctrl_ != nullptr) ctrl_->reset_key_list();
}

DrinkInfo* Entity::holding_drink() const {
  return holding != nullptr ? holding->drink.get() : nullptr;
}

void Entity::holding_set_hp(const Value& v) {
  if (holding != nullptr) holding->set_hp(to_number(v));
}

void Entity::holding_set_hp_r(const Value& v) {
  if (holding != nullptr) holding->set_hp_r(to_number(v));
}

void Entity::holding_set_velocity(const Value& x, const Value& y, const Value& z) {
  if (holding != nullptr) holding->set_velocity(x, y, z);
}

void Entity::holding_mt_mark(const std::u16string& mark) {
  (*world().lfw().mt()).mark = mark;
}

Value Entity::holding_mt_range(double lo, double hi) {
  return Value((*world().lfw().mt()).range(lo, hi));
}void Entity::holding_set_team(const Value& v) {
  if (holding != nullptr) holding->set_team(to_string(v));
}

bool Entity::holding_is_weapon() const {
  return holding != nullptr && entity::is_weapon(*holding);
}

void Entity::catcher_drop_catching() {
  if (catcher != nullptr) catcher->drop_catching();
}

Value Entity::bearer_motionless() const {
  return bearer != nullptr ? Value(bearer->motionless) : Value();
}

void Entity::set_bearer_motionless(const Value& v) {
  if (bearer != nullptr) bearer->motionless = to_number(v);
}

Value Entity::lfw_new_team() const { return Value(world().lfw().new_team()); }

Value Entity::datas_find(const std::u16string& oid) {
  return world().lfw().datas_find(Value(std::u16string(oid)));
}

Value Entity::datas_find_fighter(const std::u16string& oid) {
  return world().lfw().datas_fighters_find(Value(std::u16string(oid)));
}

Value Entity::frame_id() const { return frame.id(); }

Value Entity::cpoint_backhurtact() const {
  return frame.cpoint().valid() ? Value(frame.cpoint().backhurtact()) : Value();
}

Value Entity::cpoint_fronthurtact() const {
  return frame.cpoint().valid() ? Value(frame.cpoint().fronthurtact()) : Value();
}

Value Entity::data_in_the_skys_first() const {
  const Value list = data_indexes_of(u"in_the_skys");
  if (const Array* a = as_array(list)) return a->empty() ? Value() : a->at(0);
  return field_or(list, u"0");
}

Value Entity::holding_base_type() const {
  return holding == nullptr ? Value() : Value(holding->base_type());
}

Value Entity::fuse_bys_of() const {
  if (fuse_bys.empty()) return Value();
  std::vector<Value> ids;
  ids.reserve(fuse_bys.size());
  for (Entity* e : fuse_bys) ids.push_back(Value(e->id));
  return Value(std::make_shared<Array>(std::move(ids)));
}

void Entity::set_fuse_bys(const Value& v) {
  fuse_bys.clear();
  const Array* arr = as_array(v);
  if (arr == nullptr) {
    has_fuse_bys = false;
    return;
  }
  for (size_t i = 0; i < arr->size(); ++i) {
    Entity* e = world().find_entity(to_string(arr->at(i)));
    if (e != nullptr) fuse_bys.push_back(e);
  }
  has_fuse_bys = !fuse_bys.empty();
}

void Entity::set_dismiss_time(const Value& v) {
  if (std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v)) {
    dismiss_time = std::nullopt;
    return;
  }
  dismiss_time = to_number(v);
}

void Entity::play_sound(const Value& sounds, const Value& pos) {
  if (to_number(frame.state()) != static_cast<double>(StateEnum::Message)) {
    world().play_sound(sounds, pos);
    return;
  }
  const double centerx = to_number(frame.centerx());
  const double width = to_number(frame.width());
  double cam_x = world().camera().position.x;
  const Background* const bg = world().bg();
  const double zoom_x = bg != nullptr ? bg->zoom_x() : 1.0;
  double cam_r = cam_x + world().dataset.screen_w() / zoom_x;
  const double offset_x = facing == 1 ? centerx : width - centerx;
  cam_r -= width - offset_x;
  cam_x += offset_x;
  Object o;
  o.set(u"x", Value(clamp(to_number(field_or(pos, u"x")), cam_x, cam_r)));
  o.set(u"y", field_or(pos, u"y"));
  o.set(u"z", field_or(pos, u"z"));
  world().play_sound(sounds, Value(std::make_shared<Object>(o)));
}

void Entity::play_sound(const Value& sounds) {
  play_sound(sounds, position_value(position));
}

void Entity::spark_point(const collision::Cube& a, const collision::Cube& b, double& x, double& y,
                         double& z) const {
  Bounding rb{};
  rb.left = a.left;
  rb.right = a.right;
  rb.top = a.top;
  rb.bottom = a.bottom;
  rb.near = a.near;
  rb.far = a.far;
  Bounding rc{};
  rc.left = b.left;
  rc.right = b.right;
  rc.top = b.top;
  rc.bottom = b.bottom;
  rc.near = b.near;
  rc.far = b.far;
  const Bounding cross = cross_bounding(rb, rc);
  (*world().lfw().mt()).mark = u"sp_1";
  x = (*world().lfw().mt()).range(cross.left, cross.right);
  y = 2 + round_float((cross.bottom + cross.top) / 2);
  z = max(cross.far, cross.near) + 3;
}

void Entity::set_prop(const std::u16string& name, const Value& v) {
  if (name == u"hp") {
    set_hp(to_number(v));
  } else if (name == u"hp_r") {
    set_hp_r(to_number(v));
  } else if (name == u"hp_max") {
    set_hp_max(to_number(v));
  } else if (name == u"mp") {
    set_mp(to_number(v));
  } else if (name == u"mp_max") {
    set_mp_max(to_number(v));
  } else if (name == u"invisible") {
    set_invisible(to_number(v));
  } else if (name == u"invulnerable") {
    set_invulnerable(to_number(v));
  } else if (name == u"toughness") {
    set_toughness(to_number(v));
  } else if (name == u"fallinjury") {
    fallinjury = to_number(v);
  } else if (name == u"throwinjury") {
    throwinjury = to_number(v);
  } else if (name == u"motionless") {
    set_motionless(to_number(v));
  } else if (name == u"shaking") {
    set_shaking(to_number(v));
  } else if (name == u"facing") {
    set_facing(v);
  } else if (name == u"arest") {
    set_arest(to_number(v));
  } else if (name == u"dropping") {
    set_dropping(truthy(v));
  } else if (name == u"team") {
    set_team(to_string(v));
  } else if (name == u"reserve") {
    set_reserve(to_number(v));
  } else if (name == u"holding") {
    // 数据里只有缺省 value 的写法，语义是断开引用（例：书本被拾取后解除持有）
    if (nullish(v) || is_num(v)) holding = nullptr;
  } else if (name == u"bearer") {
    if (nullish(v) || is_num(v)) bearer = nullptr;
  }
}

Entity& Entity::set_shaking(double v) {
  shaking = v;
  return *this;
}

Entity& Entity::set_motionless(double v) {
  motionless = v;
  return *this;
}

Entity& Entity::set_catcher(Entity* v) {
  catcher = v;
  return *this;
}

Value Entity::world_dataset(const std::u16string& name) const {
  return world().dataset.get(name);
}

Value Entity::itr_fall(const Value& itr) const {
  const Value fall = field_or(itr, u"fall");
  return nullish(fall) ? dataset(u"itr_fall") : fall;
}


void Entity::toughness_recovering() {
  if (_toughness_resting > 0) {
    if (!truthy(frame.toughness_recover())) return;
    set_toughness_resting(
        clamp_add(_toughness_resting, -_atom_time, 0, _toughness_resting_max));
    return;
  }
  if (_toughness >= toughness_max()) return;
  if (!_toughness_r_tick.add(_atom_time)) return;
  set_toughness(clamp_add(_toughness, _toughness_r_value, 0, _toughness_max));
}

void Entity::fall_value_recovering() {
  if (_fall_value >= fall_value_max()) return;
  if (!_fall_r_tick.add(_atom_time)) return;
  set_fall_value(clamp_add(_fall_value, _fall_r_value, 0, fall_value_max()));
}

void Entity::defend_value_recovering() {
  if (_defend_value >= defend_value_max()) return;
  if (!_defend_r_tick.add(_atom_time)) return;
  set_defend_value(clamp_add(_defend_value, _defend_r_value, 0, defend_value_max()));
}

void Entity::stat_recovering() {
  if (_resting > 0) {
    if (!truthy(frame.stat_recover())) return;
    set_resting(clamp_add(_resting, -_atom_time, 0, resting_max()));
    return;
  }
  fall_value_recovering();
  defend_value_recovering();
}

void Entity::hp_recovering() {
  if (_hp <= 0 || _hp >= _hp_r) return;
  _hp_r_tick.set_max(to_number(dataset(u"hp_r_ticks")));
  if (!_hp_r_tick.add(_atom_time)) return;
  set_hp(min(_hp_r, _hp + to_number(dataset(u"hp_r_value"))));
}

void Entity::mp_recovering() {
  if (_hp <= 0 || _mp >= mp_max() || truthy(Value(_blinking)) ||
      truthy(Value(_invisible))) {
    return;
  }
  _mp_r_tick.set_max(to_number(dataset(u"mp_r_ticks")));
  if (!_mp_r_tick.add(_atom_time)) return;
  const double r_ratio = to_number(dataset(u"mp_r_ratio"));
  double a = hp_max();
  double b = _hp;
  if (a > 500) a = 500;
  if (b > 500) b = 500;
  const double value = 1 + round_float((a - min(r_ratio * b, a)) / 100);
  set_mp(min(mp_max(), _mp + value));
}


bool Entity::set_mark(const std::u16string& key, const std::u16string& value,
                      const std::optional<Value>& prev) {
  const Value cur = marks_get(key);
  if (!prev.has_value() || nullish(*prev) || equals(cur, *prev)) {
    marks_set(key, value);
    return true;
  }
  return false;
}

bool Entity::del_mark(const std::u16string& key, const std::optional<Value>& value) {
  const Value cur = marks_get(key);
  if (!value.has_value() || nullish(*value) || equals(cur, *value)) {
    return marks_delete(key);
  }
  return false;
}

bool Entity::is_ally(const Entity& other) const { return _team == other._team; }

Entity* Entity::get_emitter(double idx) const {
  if (!(idx >= 0) || std::floor(idx) != idx) return nullptr;
  const std::size_t i = static_cast<std::size_t>(idx);
  if (i >= emitters.size()) return nullptr;
  if (emitters[i].empty()) return nullptr;
  return world().find_entity(emitters[i]);
}


void Entity::add_v_rest(const collision::Collision& c) {
  vrests[c.aid] = c;
  const Value kind = field_or(c.itr, u"kind");
  if (strict_equals(kind, Value(static_cast<double>(ItrKind::Block)))) blockers[c.aid] = c;
  if (strict_equals(kind, Value(static_cast<double>(ItrKind::SuperPunchMe))))
    superpunchs[c.aid] = c;
}

double Entity::get_v_rest(const std::u16string& a_id) const {
  const auto it = vrests.find(a_id);
  if (it == vrests.end()) return 0;
  const double rest = it->second.rest;
  return truthy(Value(rest)) ? rest : 0;
}

void Entity::del_v_rest(const std::u16string& a_id) {
  vrests.erase(a_id);
  blockers.erase(a_id);
  superpunchs.erase(a_id);
}

double Entity::get_flag(const Entity& other) const {
  return flag_between(team(), hp(), type(), other.team());
}

void Entity::clean_holding() {
  if (holding == nullptr) return;
  if (holding->bearer == this) holding->bearer = nullptr;
  holding = nullptr;
}

void Entity::clean_catching() {
  if (catching == nullptr) return;
  if (catching->catcher == this) catching->catcher = nullptr;
  catching = nullptr;
}

bool Entity::drop_catching() {
  if (catching == nullptr) return false;
  if (catching->catcher == this) catching->catcher = nullptr;
  set_catching(nullptr);
  const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
  enter_frame(auto_frame != nullptr ? *auto_frame : Value());
  return true;
}

void Entity::release() {
  if (!truthy(Value(_mounted))) return;
  if (bearer != nullptr) bearer->drop_holding();
  if (catcher != nullptr) catcher->drop_catching();
  clean_holding();
  clean_catching();
  _mounted = 0;
  callbacks.call(u"on_disposed", {});
  callbacks.clear();
  reset(_data, states_);
  world().del_entity(*this);
}

void Entity::blink_and_gone(double duration) {
  _blinking = duration;
  _after_blink = frame_id::kGone;
}

void Entity::blink_and_respawn(double duration) {
  _blinking = duration;
  _after_blink = frame_id::kRespawn;
}

namespace {

template <typename T>
void update_hit_ground(Entity& e, const std::vector<T>& list, double position_y, double centery,
                       double ground_y) {
  for (const T& area : list) {
    const Value& target = area.on_hit_ground();
    if (!truthy(target)) continue;
    const double y = std::isnan(area.y()) ? 0.0 : area.y();
    const double h = std::isnan(area.h()) ? 0.0 : area.h();
    if (position_y + centery - y - h > ground_y) continue;
    e.enter_frame(target);
  }
}

}

void Entity::update_itr_bdy_hit_ground(const std::vector<ItrInfo>& itrs) {
  update_hit_ground(*this, itrs, position.y, frame.centery(), _ground_y);
}

void Entity::update_itr_bdy_hit_ground(const std::vector<BdyInfo>& bdys) {
  update_hit_ground(*this, bdys, position.y, frame.centery(), _ground_y);
}


namespace {

double js_length(const Value& v) {
  if (nullish(v)) return std::numeric_limits<double>::quiet_NaN();
  const Array* a = as_array(v);
  if (a != nullptr) return static_cast<double>(a->size());
  const std::u16string* s = std::get_if<std::u16string>(&v);
  if (s != nullptr) return static_cast<double>(s->size());
  return std::numeric_limits<double>::quiet_NaN();
}

Value position_value(const Vector3& p) {
  Object o;
  o.set(u"x", Value(p.x));
  o.set(u"y", Value(p.y));
  o.set(u"z", Value(p.z));
  return Value(std::make_shared<Object>(o));
}

}

void Entity::set_position(const Value& x, const Value& y, const Value& z) {
  if (!nullish(x)) position.x = round_float(to_number(x));
  if (!nullish(y)) position.y = round_float(to_number(y));
  if (!nullish(z)) position.z = round_float(to_number(z));
  if (prev_position.x == kMinSafeInteger) prev_position = position;
  const Vector3 r = world().restrict(*this);
  const NextFrame& on_x = frame.on_x_restrict();
  if (position.x != r.x && on_x) enter_frame(on_x);
  const NextFrame& on_z = frame.on_z_restrict();
  if (position.z != r.z && on_z) enter_frame(on_z);
  const NextFrame& on_y = frame.on_y_restrict();
  if (position.y != r.y && on_y) enter_frame(on_y);
  if (position.x != r.x || position.y != r.y || position.z != r.z) {
    const NextFrame& on_restrict = frame.on_restrict();
    if (on_restrict) enter_frame(on_restrict);
    if (_state != nullptr) _state->on_restrict(*this, r.x, r.y, r.z);
  }
  _ground_y = Ground::y(terrain, position.x, position.z);
}

void Entity::update_position() {
  if (bearer != nullptr || catcher != nullptr || truthy(Value(shaking)) ||
      truthy(Value(motionless)))
    return;
  double vx = velocity.x;
  double vz = velocity.z;
  const double vy = velocity.y;
  const double atom_time = _atom_time;
  for (const auto& kv : blockers) {
    const collision::Collision& v = kv.second;
    if ((vx < 0 && v.attacker.px() < position.x) || (vx > 0 && v.attacker.px() > position.x)) {
      vx = 0;
      prev_velocity.x = 0;
    }
    if ((vz < 0 && v.attacker.pz() < position.z) || (vz > 0 && v.attacker.pz() > position.z)) {
      vz = 0;
      prev_velocity.z = 0;
    }
  }
  if (!truthy(Value(shaking)) && !truthy(Value(motionless))) {
    double x = position.x;
    double y = position.y;
    double z = position.z;
    x += (vx + prev_velocity.x) * 0.5 * atom_time;
    y += (vy + prev_velocity.y) * 0.5 * atom_time;
    z += (vz + prev_velocity.z) * 0.5 * atom_time;
    set_position(Value(x), Value(y), Value(z));
  }
  prev_velocity.set(vx, vy, vz);
}

void Entity::set_frame(const FrameInfo& v) {
  _motionless_ticks = 0;
  const bool is_gone = v.id() == frame_id::kGone;
  if (is_gone) {
    opoints.clear();
  } else if (!opoints.empty()) {
    const std::vector<Opoint>& next_opoints = v.opoint();
    std::size_t slow = 0;
    for (std::size_t fast = 0; fast < opoints.size(); ++fast) {
      const Value& opoint = opoints[fast].first;
      if (!strict_equals(field_or(opoint, u"interval_mode"), Value(1.0))) continue;
      const Value interval_id = field_or(opoint, u"interval_id");
      bool exists = false;
      for (const Opoint& next_opoint : next_opoints) {
        if (strict_equals(next_opoint.interval_id(), interval_id)) {
          exists = true;
          break;
        }
      }
      if (!exists) continue;
      opoints[slow++] = opoints[fast];
    }
    opoints.resize(slow);
  }
  _prev_frame = frame;
  _landing_frame = FrameInfo();
  frame = v;
  if (v.itr().empty()) set_arest(0);
  const double prev_state_code = _prev_frame.state();
  const Value next_state_code = state();
  if (!strict_equals(Value(prev_state_code), next_state_code)) set_state(next_state_code);
  if (truthy(Value(v.invisible()))) set_invisible(v.invisible());
  if (truthy(Value(v.blinking()))) set_blinking(v.blinking());
  if (truthy(Value(v.invulnerable()))) _invulnerable = v.invulnerable();
  if (!v.opoint().empty()) apply_opoints(v.opoint());
  if (!v.cpoint().valid()) {
    set_catching(nullptr);
    catcher = nullptr;
  }
  const Array* broadcasts = as_array(v.broadcasts());
  if (broadcasts != nullptr && !broadcasts->empty()) {
    for (std::size_t i = 0; i < broadcasts->size(); ++i) world().lfw().broadcast(broadcasts->at(i));
  }
  if (holding != nullptr) holding->follow_bearer();
  if (catching != nullptr) catching->follow_catcher();
}

EnterFrameResult Entity::enter_frame(const Value& nfs, bool fallback) {
  if (strict_equals(frame.id(), Value(std::u16string(frame_id::kGone))))
    return EnterFrameResult::Gone;
  const std::optional<ResolvedNextFrame> result = get_next_frame(nfs);
  if (!result.has_value() && fallback) {
    const FrameInfo* const f = find_auto_frame();
    set_frame(*f);
    wait = handle_wait_flag(Value(), f);
    return EnterFrameResult::Fallback;
  }
  if (!result.has_value()) return EnterFrameResult::NotFound;
  return handle_next_frame_result(*result);
}

EnterFrameResult Entity::enter_frame(const NextFrame& nfs, bool fallback) {
  return enter_frame(nfs.to_value(), fallback);
}

EnterFrameResult Entity::enter_frame_by_id(const Value& id_value, bool fallback) {
  Value use_id = id_value;
  if (nullish(use_id) && fallback) use_id = Value(std::u16string(frame_id::kAuto));
  Object* nf = as_object(_next_frame_by_id);
  if (nf != nullptr) nf->set(u"id", use_id);
  return enter_frame(_next_frame_by_id, fallback);
}

EnterFrameResult Entity::handle_next_frame_result(const ResolvedNextFrame& result, bool fallback) {
  const FrameInfo* const frame_v = result.frame;
  const Value& flags = result.which;
  if (!truthy(Value(world().dataset.infinity_mp()))) {
    const Value mp = field_or(flags, u"mp");
    const Value hp = field_or(flags, u"hp");
    if (truthy(mp)) set_mp(_mp - to_number(mp));
    if (truthy(hp)) set_hp(_hp - to_number(hp));
  }
  if (frame_v != nullptr) {
    play_sound(frame_v->sound().to_value());
    set_frame(*frame_v);
  } else {
    if (frame.same_as(empty_frame_info()) || fallback) set_frame(*find_auto_frame());
  }
  const Value facing_flag = field_or(flags, u"facing");
  if (!std::holds_alternative<std::monostate>(facing_flag))
    facing = handle_facing_flag(facing_flag);
  if (frame_v != nullptr) wait = handle_wait_flag(field_or(flags, u"wait"), frame_v);
  const Value sound = field_or(flags, u"sound");
  if (as_array(sound) != nullptr) play_sound(sound);
  const Value blink_time = field_or(flags, u"blink_time");
  if (truthy(blink_time)) set_blinking(to_number(blink_time));
  if (truthy(field_or(flags, u"reset_keys")) && ctrl_ != nullptr) ctrl_->reset_key_list();
  if (truthy(field_or(flags, u"transfrom_to_another"))) transfrom_to_another(std::nullopt);
  return frame_v != nullptr ? EnterFrameResult::Entered : EnterFrameResult::Fallback;
}

bool Entity::has_next_frame_judge(const Value& nf) const {
  return loader::judger_of(nf) != nullptr;
}

Value Entity::next_frame_judge(const Value& nf) {
  Expression<Entity>* const judger = loader::judger_of(nf);
  if (judger == nullptr) return Value();
  return Value(judger->run(*this));
}

std::optional<ResolvedNextFrame> Entity::get_next_frame(const Value& which) {
  const Array* arr = as_array(which);
  if (arr != nullptr) {
    const std::size_t l = arr->size();
    std::vector<Value> remains;
    for (std::size_t i = 0; i < l; ++i) {
      const Value nf = arr->at(i);
      if (!truthy(nf)) continue;
      if (!has_next_frame_judge(nf)) {
        remains.push_back(nf);
        continue;
      }
      std::optional<ResolvedNextFrame> f = get_next_frame(nf);
      if (f.has_value()) return f;
    }
    Array remains_arr(remains);
    (*world().lfw().mt()).mark = u"gnf_0";
    const Value next = (*world().lfw().mt()).pick_value(Value(std::make_shared<Array>(remains_arr)));
    if (nullish(next)) return std::nullopt;
    return get_next_frame(next);
  }
  const NextFrame nf(which);
  const Value& id_value = nf.id_value();
  const double use_hp = nf.hp();
  const double use_mp = nf.mp();
  const double mp_mode = nf.mp_mode();
  if (has_next_frame_judge(which) && !truthy(next_frame_judge(which))) return std::nullopt;
  const FrameInfo* found_frame = nullptr;
  if (truthy(id_value)) {
    (*world().lfw().mt()).mark = u"gnf_1";
    found_frame = find_frame_by_id((*world().lfw().mt()).pick_value(id_value));
  }
  if (!truthy(Value(world().dataset.infinity_mp())) && found_frame != nullptr) {
    const Value frame_next = frame.next().to_value();
    const Value hit_d = found_frame->hit().d();
    const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
    const Value fallback_frame =
        or_nullish(hit_d, auto_frame != nullptr ? *auto_frame : Value());
    if (same_ref(frame_next, which)) {
      if (truthy(Value(use_mp)) && _mp < use_mp) return get_next_frame(fallback_frame);
      if (truthy(Value(use_hp)) && _hp <= use_hp) return get_next_frame(fallback_frame);
    } else {
      if (truthy(Value(use_mp)) && _mp < use_mp && mp_mode != 1.0) return std::nullopt;
      if (truthy(Value(use_hp)) && _hp <= use_hp) return std::nullopt;
    }
  }
  Value w;
  if (is_str(which)) {
    Object o;
    o.set(u"id", which);
    w = Value(std::make_shared<Object>(o));
  } else {
    w = which;
  }
  return ResolvedNextFrame{found_frame, w};
}


void Entity::follow_bearer() {
  Entity* const bearer_src = bearer;
  if (bearer_src == nullptr) return;
  set_team(bearer_src->team());
  if (_hp <= 0 && bearer_src != nullptr) {
    drop_holding();
    return;
  }
  const Wpoint& wp_a = bearer_src->frame.wpoint();
  const double cx_a = to_number(bearer_src->frame.centerx());
  const double cy_a = to_number(bearer_src->frame.centery());
  if (equals(Value(wp_a.kind()), Value(static_cast<double>(WpointKind::Drop)))) {
    bearer_src->drop_holding();
    (*world().lfw().mt()).mark = u"dh_v";
    const double vy = 3;
    MersenneTwister& mt = (*world().lfw().mt());
    const double vx = mt.range(-10, 10) / 10;
    const double vz = mt.range(-10, 10) / 20;
    set_velocity(Value(vx), Value(vy), Value(vz));
    return;
  }
  const std::u16string& weaponact = wp_a.weaponact();
  if (!strict_equals(Value(weaponact), frame.id())) {
    enter_frame_by_id(Value(weaponact), true);
  }
  const Wpoint& wp_b = frame.wpoint();
  const double cx_b = to_number(frame.centerx());
  const double cy_b = to_number(frame.centery());
  const double weight = truthy(field_or(_data.base(), u"weight"))
                            ? to_number(field_or(_data.base(), u"weight"))
                            : 1;
  const bool has_dvx = !std::isnan(wp_a.dvx());
  const bool has_dvy = !std::isnan(wp_a.dvy());
  const bool has_dvz = !std::isnan(wp_a.dvz());
  const Value dvx = has_dvx ? Value(wp_a.dvx()) : Value();
  const Value dvy = has_dvy ? Value(wp_a.dvy()) : Value();
  const Value dvz = has_dvz ? Value(wp_a.dvz()) : Value();
  const double x = bearer_src->position.x;
  const double y = bearer_src->position.y;
  const double z = bearer_src->position.z;
  facing = bearer_src->facing;
  const double wa_x = (std::isnan(wp_a.x()) ? 0.0 : wp_a.x());
  const double wa_y = (std::isnan(wp_a.y()) ? 0.0 : wp_a.y());
  const double wa_z = (std::isnan(wp_a.z()) ? 0.0 : wp_a.z());
  const double wb_x = (std::isnan(wp_b.x()) ? 0.0 : wp_b.x());
  const double wb_y = (std::isnan(wp_b.y()) ? 0.0 : wp_b.y());
  const double wb_z = (std::isnan(wp_b.z()) ? 0.0 : wp_b.z());
  if (truthy(Value(wp_a.kind()))) {
    prev_position = bearer_src->position;
    set_position(Value(x + facing * (wa_x - cx_a + cx_b - wb_x)),
                 Value(y + cy_a - wa_y - cy_b + wb_y), Value(z + wa_z - wb_z));
  } else {
    set_position(Value(x + facing * (wa_x - cx_a)), Value(y + cy_a - wa_y),
                 Value(z + wa_z));
  }
  if (!std::holds_alternative<std::monostate>(dvx) ||
      !std::holds_alternative<std::monostate>(dvy) ||
      !std::holds_alternative<std::monostate>(dvz)) {
    bearer_src->holding = nullptr;
    bearer = nullptr;
    dropping = false;
    const double fvx = truthy(dvx) ? to_number(dvx) * to_number(dataset(u"wvx_f")) : 0;
    const double fvy = truthy(dvy) ? to_number(dvy) * to_number(dataset(u"wvy_f")) : 0;
    const double fvz = truthy(dvz) ? to_number(dvz) * to_number(dataset(u"wvz_f")) : 0;
    const Value nf = find_align_frame(
        frame_id_value(frame), field_or(_data.indexes(), u"on_hands"),
        field_or(_data.indexes(), u"throwings"));
    prev_position = position;
    set_position(Value(round(x + facing * (wa_x - cx_a))),
                 Value(round(y + cy_a - wa_y)), Value(round(z + wa_z)));
    const double vz = bearer_src->ctrl() != nullptr
                          ? static_cast<double>(bearer_src->ctrl()->UD()) * fvz
                          : 0;
    const double dvx_w = fvx / weight;
    const double dvy_w = fvy / weight;
    const double vx = (dvx_w - abs(vz / 2)) * facing;
    set_velocity(Value(vx), Value(dvy_w), Value(vz));
    enter_frame(nf);
    return;
  }
}

void Entity::follow_catcher() {
  Entity* a = catcher;
  Entity* b = this;
  if (a == nullptr) return;
  const Cpoint& ac = a->frame.cpoint();
  if (!ac.valid()) return;
  const double afx = to_number(a->frame.centerx());
  const double afy = to_number(a->frame.centery());
  const double tx = (std::isnan(ac.throwvx()) ? 0.0 : ac.throwvx());
  const double ty = (std::isnan(ac.throwvy()) ? 0.0 : ac.throwvy());
  const double tz = (std::isnan(ac.throwvz()) ? 0.0 : ac.throwvz());
  const double bfx = to_number(b->frame.centerx());
  const double bfy = to_number(b->frame.centery());
  const Cpoint& bc = b->frame.cpoint();
  const double ax = a->position.x;
  const double ay = a->position.y;
  const double az = a->position.z;
  const double a_face = a->facing;
  const double acx = (std::isnan(ac.x()) ? 0.0 : ac.x());
  const double acy = (std::isnan(ac.y()) ? 0.0 : ac.y());
  const double acz = (std::isnan(ac.z()) ? 0.0 : ac.z());
  if (truthy(Value(tx)) || truthy(Value(ty)) || truthy(Value(tz))) {
    const double vx = tx * to_number(dataset(u"tvx_f")) * a_face;
    const double vy = ty * to_number(dataset(u"tvy_f"));
    const double vz = tz * to_number(dataset(u"tvz_f")) *
                      static_cast<double>(a->ctrl() != nullptr ? a->ctrl()->UD() : 0);
    set_velocity(Value(vx), Value(vy), Value(vz));
    set_position(Value((2 * vx) + ax - a_face * (afx - acx)),
                 Value((2 * vy) + ay + afy - acy), Value((2 * vz) + az + acz));
    return;
  }
  const double b_face = b->facing;
  const double bcx = (std::isnan(bc.x()) ? 0.0 : bc.x());
  const double bcy = (std::isnan(bc.y()) ? 0.0 : bc.y());
  const double bcz = (std::isnan(bc.z()) ? 0.0 : bc.z());
  set_position(Value(ax - a_face * (afx - acx) + b_face * (bfx - bcx)),
               Value(ay + afy - acy + bcy - bfy), Value(az + acz - bcz));
}

void Entity::drop_holding() {
  if (holding == nullptr) return;
  (*world().lfw().mt()).mark = u"dh_1";
  Entity* held = holding;
  held->bearer = nullptr;
  holding = nullptr;
  held->dropping = true;
  const Value on_hands = field_or(held->data().indexes(), u"on_hands");
  const Value in_the_skys = field_or(held->data().indexes(), u"in_the_skys");
  Value nf = held->find_align_frame(frame_id_value(held->frame), on_hands, in_the_skys);
  if (!truthy(nf)) {
    Object o;
    o.set(u"id", Value(std::u16string(frame_id::kAuto)));
    nf = Value(std::make_shared<Object>(o));
  }
  held->enter_frame(nf);
  held->set_position(Value(held->position.x), Value(held->position.y),
                     Value(held->position.z));
  held->set_team(team());
  for (const auto& kv : vrests) {
    collision::Collision clone = kv.second;
    held->add_v_rest(clone);
  }
}

void Entity::pick(Entity& weapon) {
  if (weapon.bearer != nullptr) return;
  if (holding != nullptr) return;
  holding = &weapon;
  weapon.bearer = this;
  weapon.dropping = false;
  weapon.follow_bearer();
  std::shared_ptr<Summary> s = summary_mgr().get(id);
  s->set_picking_sum(Value(to_number(s->picking_sum()) + 1));
  if (!defines::is_independent(team())) {
    std::shared_ptr<Summary> t = summary_mgr().get(team());
    t->set_picking_sum(Value(to_number(t->picking_sum()) + 1));
  }
}

void Entity::transform(const EntityData& data) {
  if (ctrl_ == nullptr || !ctrl_->is_human()) {
    controller::BaseController* c = world().lfw().create_ctrl(data.id(), ctrl_ != nullptr ? ctrl_->player_id : std::u16string());
    set_ctrl(c);
  }
  const Value prev = _data.to_value();
  _data = data;
  reset_armor();
  callbacks.call(u"on_data_changed", {_data.to_value(), prev});
}

bool Entity::transfrom_to_another(const std::optional<EntityData>& data) {
  if (data.has_value()) {
    transforms.clear();
    transforms.push_back(_data);
    transforms.push_back(*data);
  }
  if (transforms.empty()) return false;
  const double curr_idx = transform_index;
  const size_t len = transforms.size();
  const double next_idx_f = std::fmod(curr_idx + 1, static_cast<double>(len));
  const size_t next_idx = static_cast<size_t>(next_idx_f);
  const EntityData& next_data = transforms[next_idx];
  if (!next_data.valid()) return false;
  transform_index = next_idx_f;
  transform(next_data);
  if (next_idx == 0) enter_frame_by_id(Value(std::u16string(u"245")), true);
  if (!copies.empty()) {
    std::vector<std::u16string> gones;
    for (const std::u16string& cid : copies) {
      Entity* copy = world().find_entity(cid);
      if (copy == nullptr || !truthy(Value(copy->mounted()))) {
        gones.push_back(cid);
      } else {
        copy->transform(next_data);
      }
    }
    for (const std::u16string& gone_id : gones) {
      for (std::size_t i = 0; i < copies.size(); ++i) {
        if (copies[i] == gone_id) {
          copies.erase(copies.begin() + static_cast<std::ptrdiff_t>(i));
          break;
        }
      }
    }
  }
  return true;
}


void Entity::to_snapshot(std::vector<Value>& nums, std::vector<std::u16string>& strs) const {
  using entity::NSlot;
  using entity::SSlot;
  const auto N = [&nums](NSlot slot) -> Value& {
    return nums[static_cast<std::size_t>(slot)];
  };
  const auto S = [&strs](SSlot slot) -> std::u16string& {
    return strs[static_cast<std::size_t>(slot)];
  };

  N(NSlot::WAIT) = Value(wait);
  N(NSlot::VARIANT) = Value(variant);
  N(NSlot::TRANSFORM_INDEX) = Value(transform_index);
  N(NSlot::LIFETIME) = Value(_lifetime);
  N(NSlot::SPAWN_TIME) = Value(_spawn_time);

  N(NSlot::RESERVE) = Value(_reserve);
  N(NSlot::MOUNTED) = Value(_mounted);
  N(NSlot::GHOSTED) = Value(_ghosted);

  N(NSlot::RESTING) = Value(_resting);
  N(NSlot::RESTING_MAX) = or_nan(_resting_max);
  N(NSlot::TOUGHNESS) = Value(_toughness);
  N(NSlot::TOUGHNESS_MAX) = Value(_toughness_max);
  N(NSlot::TOUGHNESS_R_VALUE) = Value(_toughness_r_value);
  N(NSlot::TOUGHNESS_RESTING) = Value(_toughness_resting);
  N(NSlot::TOUGHNESS_RESTING_MAX) = Value(_toughness_resting_max);

  N(NSlot::FALL_VALUE) = Value(_fall_value);
  N(NSlot::FALL_VALUE_MAX) = or_nan(_fall_value_max);
  N(NSlot::FALL_R_VALUE) = Value(_fall_r_value);
  N(NSlot::DEFEND_VALUE) = Value(_defend_value);
  N(NSlot::DEFEND_VALUE_MAX) = or_nan(_defend_value_max);
  N(NSlot::DEFEND_R_VALUE) = Value(_defend_r_value);
  N(NSlot::DEFEND_RATIO) = or_nan(_defend_ratio);

  N(NSlot::FALLINJURY) = Value(fallinjury);
  N(NSlot::THROWINJURY) = Value(throwinjury);
  N(NSlot::FACING) = Value(facing);

  N(NSlot::POS_X) = Value(position.x);
  N(NSlot::POS_Y) = Value(position.y);
  N(NSlot::POS_Z) = Value(position.z);
  N(NSlot::PREV_POS_X) = Value(prev_position.x);
  N(NSlot::PREV_POS_Y) = Value(prev_position.y);
  N(NSlot::PREV_POS_Z) = Value(prev_position.z);
  N(NSlot::VEL_X) = Value(velocity.x);
  N(NSlot::VEL_Y) = Value(velocity.y);
  N(NSlot::VEL_Z) = Value(velocity.z);
  N(NSlot::PREV_VEL_X) = Value(prev_velocity.x);
  N(NSlot::PREV_VEL_Y) = Value(prev_velocity.y);
  N(NSlot::PREV_VEL_Z) = Value(prev_velocity.z);

  N(NSlot::MP) = Value(_mp);
  N(NSlot::MP_MAX) = _mp_max.has_value() ? Value(*_mp_max) : Value(NullTag{});
  N(NSlot::HP) = Value(_hp);
  N(NSlot::HP_R) = Value(_hp_r);
  N(NSlot::HP_MAX) = _hp_max.has_value() ? Value(*_hp_max) : Value(NullTag{});

  N(NSlot::AREST) = Value(_arest);
  N(NSlot::MOTIONLESS) = Value(motionless);
  N(NSlot::SHAKING) = Value(shaking);

  N(NSlot::CATCH_TIME) = Value(_catch_time);
  N(NSlot::CATCH_TIME_MAX) = or_nan(_catch_time_max);
  N(NSlot::DISMISS_TIME) = or_nan(dismiss_time);

  N(NSlot::INVISIBLE_DURATION) = Value(_invisible);
  N(NSlot::INVULNERABLE_DURATION) = Value(_invulnerable);
  N(NSlot::BLINKING_DURATION) = Value(_blinking);

  N(NSlot::JUMP_X) = Value(jumping.x);
  N(NSlot::JUMP_Y) = Value(jumping.y);
  N(NSlot::JUMP_Z) = Value(jumping.z);
  N(NSlot::JUMP_T) = Value(jumping.t);

  N(NSlot::GROUND_Y) = Value(_ground_y);
  N(NSlot::PREV_GROUND_Y) = Value(_prev_ground_y);

  N(NSlot::AABB_MIN_X) = Value(aabb_min_x);
  N(NSlot::AABB_MAX_X) = Value(aabb_max_x);
  N(NSlot::AABB_MIN_Z) = Value(aabb_min_z);
  N(NSlot::AABB_MAX_Z) = Value(aabb_max_z);
  N(NSlot::L_LEN) = Value(l_len);
  N(NSlot::R_LEN) = Value(r_len);

  N(NSlot::STAT_BAR_TYPE) = Value(stat_bar);

  write_tick(_hp_r_tick, nums, NSlot::HP_R_TICK_VALUE);
  write_tick(_mp_r_tick, nums, NSlot::MP_R_TICK_VALUE);
  write_tick(_resting_tick, nums, NSlot::RESTING_TICK_VALUE);
  write_tick(_toughness_r_tick, nums, NSlot::TOUGHNESS_R_TICK_VALUE);
  write_tick(_fall_r_tick, nums, NSlot::FALL_R_TICK_VALUE);
  write_tick(_defend_r_tick, nums, NSlot::DEFEND_R_TICK_VALUE);

  N(NSlot::BOUNCED) = Value(bounced ? 1.0 : 0.0);
  N(NSlot::LYING_A_COUNT) = lying_a_count;
  N(NSlot::LYING_D_COUNT) = lying_d_count;
  N(NSlot::LYING_C_COUNT) = lying_c_count;
  N(NSlot::DROP_HURTED) = Value(drop_hurted ? 1.0 : 0.0);
  N(NSlot::DROPPING) = Value(dropping ? 1.0 : 0.0);
  N(NSlot::IS_ON_GROUND) = Value(is_on_ground ? 1.0 : 0.0);
  N(NSlot::NAME_VISIBLE) = Value(name_visible);
  N(NSlot::WAKEUP_INVULN) = Value(wakeup_invuln);
  N(NSlot::DEAD_GONE) = Value(dead_gone);
  N(NSlot::CTRL_VISIBLE) = Value(ctrl_visible);

  S(SSlot::ID) = id;
  S(SSlot::DATA_ID) = _data.id();
  S(SSlot::FRAME_ID) = frame_id_of(*this);
  S(SSlot::PREV_FRAME_ID) = frame_id_value(_prev_frame);
  S(SSlot::LANDING_FRAME_ID) =
      _landing_frame.valid() ? frame_id_value(_landing_frame) : std::u16string();
  S(SSlot::CATCHING_ID) = catching != nullptr ? catching->id : std::u16string();
  S(SSlot::CATCHER_ID) = catcher != nullptr ? catcher->id : std::u16string();
  S(SSlot::BEARER_ID) = bearer != nullptr ? bearer->id : std::u16string();
  S(SSlot::HOLDING_ID) = holding != nullptr ? holding->id : std::u16string();
  S(SSlot::TEAM) = _team;
  S(SSlot::NAME) = nullish(_name) ? std::u16string() : to_string(_name);
  S(SSlot::AFTER_BLINK) = _after_blink.has_value() ? *_after_blink : std::u16string();
  S(SSlot::DISMISS_DATA_ID) =
      dismiss_data.has_value() ? dismiss_data->id() : std::u16string();
  S(SSlot::TRANSFORM_0_ID) =
      transforms.size() > 0 ? transforms[0].id() : std::u16string();
  S(SSlot::TRANSFORM_1_ID) =
      transforms.size() > 1 ? transforms[1].id() : std::u16string();
  std::u16string copy_list;
  for (const std::u16string& copy_id : copies) copy_list += copy_id + u",";
  S(SSlot::COPIES) = copies.empty() ? std::u16string() : copy_list.substr(0, copy_list.size() - 1);
  S(SSlot::DEAD_JOIN) = truthy(dead_join) ? json_stringify(dead_join).value_or(u"")
                                          : std::u16string();
}

void Entity::read_snapshot(const std::vector<Value>& nums,
                           const std::vector<std::u16string>& strs) {
  using entity::NSlot;
  using entity::SSlot;
  const auto N = [&nums](NSlot slot) -> const Value& {
    return nums[static_cast<std::size_t>(slot)];
  };
  const auto S = [&strs](SSlot slot) -> const std::u16string& {
    return strs[static_cast<std::size_t>(slot)];
  };

  wait = to_number(N(NSlot::WAIT));
  variant = to_number(N(NSlot::VARIANT));
  transform_index = to_number(N(NSlot::TRANSFORM_INDEX));
  _lifetime = to_number(N(NSlot::LIFETIME));
  _spawn_time = to_number(N(NSlot::SPAWN_TIME));

  _reserve = to_number(N(NSlot::RESERVE));
  _mounted = to_number(N(NSlot::MOUNTED));
  _ghosted = to_number(N(NSlot::GHOSTED));

  _resting = to_number(N(NSlot::RESTING));
  _resting_max = opt_num(entity::num_or_null(N(NSlot::RESTING_MAX)));
  _toughness = to_number(N(NSlot::TOUGHNESS));
  _toughness_max = to_number(N(NSlot::TOUGHNESS_MAX));
  _toughness_r_value = to_number(N(NSlot::TOUGHNESS_R_VALUE));
  _toughness_resting = to_number(N(NSlot::TOUGHNESS_RESTING));
  _toughness_resting_max = to_number(N(NSlot::TOUGHNESS_RESTING_MAX));

  _fall_value = to_number(N(NSlot::FALL_VALUE));
  _fall_value_max = opt_num(entity::num_or_null(N(NSlot::FALL_VALUE_MAX)));
  _fall_r_value = to_number(N(NSlot::FALL_R_VALUE));
  _defend_value = to_number(N(NSlot::DEFEND_VALUE));
  _defend_value_max = opt_num(entity::num_or_null(N(NSlot::DEFEND_VALUE_MAX)));
  _defend_r_value = to_number(N(NSlot::DEFEND_R_VALUE));
  _defend_ratio = opt_num(entity::num_or_null(N(NSlot::DEFEND_RATIO)));

  fallinjury = to_number(N(NSlot::FALLINJURY));
  throwinjury = to_number(N(NSlot::THROWINJURY));
  facing = to_number(N(NSlot::FACING));

  position.set(to_number(N(NSlot::POS_X)), to_number(N(NSlot::POS_Y)),
               to_number(N(NSlot::POS_Z)));
  prev_position.set(to_number(N(NSlot::PREV_POS_X)), to_number(N(NSlot::PREV_POS_Y)),
                    to_number(N(NSlot::PREV_POS_Z)));
  velocity.set(to_number(N(NSlot::VEL_X)), to_number(N(NSlot::VEL_Y)),
               to_number(N(NSlot::VEL_Z)));
  prev_velocity.set(to_number(N(NSlot::PREV_VEL_X)), to_number(N(NSlot::PREV_VEL_Y)),
                    to_number(N(NSlot::PREV_VEL_Z)));

  _mp = to_number(N(NSlot::MP));
  _mp_max = opt_num(N(NSlot::MP_MAX));
  _hp = to_number(N(NSlot::HP));
  _hp_r = to_number(N(NSlot::HP_R));
  _hp_max = opt_num(N(NSlot::HP_MAX));

  _arest = to_number(N(NSlot::AREST));
  motionless = to_number(N(NSlot::MOTIONLESS));
  shaking = to_number(N(NSlot::SHAKING));

  _catch_time = to_number(N(NSlot::CATCH_TIME));
  _catch_time_max = opt_num(entity::num_or_null(N(NSlot::CATCH_TIME_MAX)));
  dismiss_time = opt_num(entity::num_or_null(N(NSlot::DISMISS_TIME)));

  _invisible = to_number(N(NSlot::INVISIBLE_DURATION));
  _invulnerable = to_number(N(NSlot::INVULNERABLE_DURATION));
  _blinking = to_number(N(NSlot::BLINKING_DURATION));

  jumping.x = to_number(N(NSlot::JUMP_X));
  jumping.y = to_number(N(NSlot::JUMP_Y));
  jumping.z = to_number(N(NSlot::JUMP_Z));
  jumping.t = to_number(N(NSlot::JUMP_T));

  _ground_y = to_number(N(NSlot::GROUND_Y));
  _prev_ground_y = to_number(N(NSlot::PREV_GROUND_Y));

  aabb_min_x = to_number(N(NSlot::AABB_MIN_X));
  aabb_max_x = to_number(N(NSlot::AABB_MAX_X));
  aabb_min_z = to_number(N(NSlot::AABB_MIN_Z));
  aabb_max_z = to_number(N(NSlot::AABB_MAX_Z));
  l_len = to_number(N(NSlot::L_LEN));
  r_len = to_number(N(NSlot::R_LEN));
  stat_bar = to_number(N(NSlot::STAT_BAR_TYPE));

  read_tick(_hp_r_tick, nums, NSlot::HP_R_TICK_VALUE);
  read_tick(_mp_r_tick, nums, NSlot::MP_R_TICK_VALUE);
  read_tick(_resting_tick, nums, NSlot::RESTING_TICK_VALUE);
  read_tick(_toughness_r_tick, nums, NSlot::TOUGHNESS_R_TICK_VALUE);
  read_tick(_fall_r_tick, nums, NSlot::FALL_R_TICK_VALUE);
  read_tick(_defend_r_tick, nums, NSlot::DEFEND_R_TICK_VALUE);

  bounced = not_zero(N(NSlot::BOUNCED));
  lying_a_count = N(NSlot::LYING_A_COUNT);
  lying_d_count = N(NSlot::LYING_D_COUNT);
  lying_c_count = N(NSlot::LYING_C_COUNT);
  drop_hurted = not_zero(N(NSlot::DROP_HURTED));
  dropping = not_zero(N(NSlot::DROPPING));
  is_on_ground = not_zero(N(NSlot::IS_ON_GROUND));
  name_visible = to_number(N(NSlot::NAME_VISIBLE));
  wakeup_invuln = to_number(N(NSlot::WAKEUP_INVULN));
  dead_gone = to_number(N(NSlot::DEAD_GONE));
  ctrl_visible = to_number(N(NSlot::CTRL_VISIBLE));

  id = S(SSlot::ID);
  const Value data = world().lfw().datas_find(Value(std::u16string(S(SSlot::DATA_ID))));
  if (truthy(data)) _data = world().lfw().datas().resolve_entity_data(data);
  const FrameInfo* const next_frame = find_frame_by_id(Value(S(SSlot::FRAME_ID)));
  if (next_frame != nullptr) frame = *next_frame;
  const FrameInfo* const next_prev = find_frame_by_id(Value(S(SSlot::PREV_FRAME_ID)));
  if (next_prev != nullptr) _prev_frame = *next_prev;
  if (S(SSlot::LANDING_FRAME_ID).empty()) {
    _landing_frame = FrameInfo();
  } else {
    const FrameInfo* const landing = find_frame_by_id(Value(S(SSlot::LANDING_FRAME_ID)));
    _landing_frame = landing != nullptr ? *landing : FrameInfo();
  }
  catching = world().find_entity(S(SSlot::CATCHING_ID));
  catcher = world().find_entity(S(SSlot::CATCHER_ID));
  bearer = world().find_entity(S(SSlot::BEARER_ID));
  holding = world().find_entity(S(SSlot::HOLDING_ID));
  _team = S(SSlot::TEAM);
  _name = S(SSlot::NAME).empty() ? Value(NullTag{}) : Value(S(SSlot::NAME));
  _after_blink = S(SSlot::AFTER_BLINK).empty()
                     ? std::nullopt
                     : std::optional<std::u16string>(S(SSlot::AFTER_BLINK));
  if (!S(SSlot::DISMISS_DATA_ID).empty()) {
    const Value found = world().lfw().datas_find(Value(std::u16string(S(SSlot::DISMISS_DATA_ID))));
    if (nullish(found)) {
      dismiss_data.reset();
    } else {
      dismiss_data = world().lfw().datas().resolve_entity_data(found);
    }
  } else {
    dismiss_data.reset();
  }

  const std::u16string& t0 = S(SSlot::TRANSFORM_0_ID);
  const std::u16string& t1 = S(SSlot::TRANSFORM_1_ID);
  transforms.clear();
  if (!t0.empty() && !t1.empty()) {
    const Value d0 = world().lfw().datas_find(Value(std::u16string(t0)));
    const Value d1 = world().lfw().datas_find(Value(std::u16string(t1)));
    if (truthy(d0) && truthy(d1)) {
      transforms.push_back(world().lfw().datas().resolve_entity_data(d0));
      transforms.push_back(world().lfw().datas().resolve_entity_data(d1));
    }
  }

  copies.clear();
  if (!S(SSlot::COPIES).empty()) {
    std::u16string cur;
    const std::u16string& all = S(SSlot::COPIES);
    for (std::size_t i = 0; i <= all.size(); ++i) {
      if (i == all.size() || all[i] == u',') {
        add_copy(cur);
        cur.clear();
      } else {
        cur.push_back(all[i]);
      }
    }
  }
  if (!S(SSlot::DEAD_JOIN).empty()) {
    const std::optional<Value> parsed = json_parse(S(SSlot::DEAD_JOIN));
    if (parsed.has_value()) dead_join = *parsed;
  } else {
    dead_join = Value(NullTag{});
  }
}

bool Entity::add_copy(const std::u16string& copy_id) {
  for (const std::u16string& existing : copies) {
    if (existing == copy_id) return false;
  }
  copies.push_back(copy_id);
  return true;
}


Value Entity::gen_or(const Value& holder, const std::u16string& kind,
                     const Value& fallback) const {
  const std::optional<Value> gen = world().gen_field(holder, kind, const_cast<Entity&>(*this));
  return gen.has_value() ? *gen : fallback;
}

namespace {
Value auto_frame_value() {
  const Value* v = defines::find(u"Defines.NEXT_FRAME_AUTO");
  return v != nullptr ? *v : Value();
}
}

Entity* Entity::spawn(const Value& opoint) {
  return spawn(opoint, Vector3{}, facing);
}

Entity* Entity::spawn(const Value& opoint, const Vector3& offset_velocity, double facing_value) {
  if (truthy(field_or(opoint, u"unimportant")) && world().entity_count() > 355) return nullptr;
  (*world().lfw().mt()).mark = u"se_1";
  const Value oid = (*world().lfw().mt()).pick_value(field_or(opoint, u"oid"));
  if (!truthy(oid)) return nullptr;
  const Value data = world().lfw().datas_find(Value(std::u16string(to_string(oid))));
  if (std::holds_alternative<std::monostate>(data)) return nullptr;
  Entity* entity = world().lfw().create_entity_with_bot(u"", world(), data);
  if (entity == nullptr) return nullptr;
  entity->on_spawn(*this, opoint, offset_velocity, facing_value).attach(field_or(opoint, u"ghost"));
  if (strict_equals(Value(entity->data().id()), Value(_data.id()))) {
    add_copy(entity->id);
  }
  entity->as_key_role(Value(false));
  for (const auto& kv : vrests) entity->add_v_rest(kv.second);
  return entity;
}

Entity& Entity::on_spawn(Entity& emitter, const Value& opoint, const Vector3& offset_velocity,
                         double facing_value) {
  const FrameInfo& emitter_frame = emitter.frame;
  if (equals(emitter.state(), Value(static_cast<double>(StateEnum::Ball_Rebounding)))) {
    std::u16string attacker_id = emitter.id;
    Value attacker_team = Value(emitter.team());
    if (emitter.lastest_collided.has_value()) {
      attacker_id = emitter.lastest_collided->attacker.id();
      attacker_team = emitter.lastest_collided->attacker.team();
    }
    emitters.clear();
    emitters.push_back(attacker_id);
    set_team(to_string(attacker_team));
    facing = emitter.facing;
  } else {
    emitters.insert(emitters.end(), emitter.emitters.begin(), emitter.emitters.end());
    emitters.push_back(emitter.id);
    set_team(emitter.team());
    facing = emitter.facing;
  }

  double pos_x = emitter.position.x;
  double pos_y = emitter.position.y;
  const double pos_z = emitter.position.z;
  const double opoint_y = num_of(or_nullish(emitter.gen_or(opoint, u"__gen_y", field_or(opoint, u"y")),
                                            Value(0.0)));
  const double opoint_x = num_of(or_nullish(emitter.gen_or(opoint, u"__gen_x", field_or(opoint, u"x")),
                                            Value(0.0)));
  const double opoint_z =
      num_of(or_nullish(emitter.gen_or(opoint, u"__gen_z", field_or(opoint, u"z")), Value(2.0)));

  if (equals(field_or(opoint, u"pos_type"), Value(1.0))) {
    pos_y = pos_y - opoint_y;
    pos_x = pos_x + emitter.facing * opoint_x;
  } else {
    pos_y = pos_y + emitter_frame.centery() - opoint_y;
    pos_x = pos_x - emitter.facing * (emitter_frame.centerx() - opoint_x);
  }
  prev_position = emitter.position;
  set_position(Value(pos_x), Value(pos_y), Value(pos_z + opoint_z));

  const std::optional<ResolvedNextFrame> result = get_next_frame(field_or(opoint, u"action"));
  const Value which = result.has_value() ? result->which : Value();
  const Value nf_facing =
      emitter.gen_or(which, u"__gen_facing", field_or(which, u"facing"));
  facing_value = truthy(nf_facing) ? handle_facing_flag(nf_facing) : emitter.facing;

  if (result.has_value()) {
    enter_frame(which);
  } else {
    enter_frame(auto_frame_value());
  }

  const Value speedz_field = field_or(opoint, u"speedz");
  const bool has_speedz = !std::holds_alternative<std::monostate>(speedz_field) &&
                          !std::holds_alternative<NullTag>(speedz_field);
  const double speedz_explicit = to_number(has_speedz ? speedz_field : Value(0.0));
  double o_dvx =
      num_of(or_nullish(emitter.gen_or(opoint, u"__gen_dvx", field_or(opoint, u"dvx")), Value(0.0)));
  double o_dvy =
      num_of(or_nullish(emitter.gen_or(opoint, u"__gen_dvy", field_or(opoint, u"dvy")), Value(0.0)));
  const double o_dvz =
      num_of(or_nullish(emitter.gen_or(opoint, u"__gen_dvz", field_or(opoint, u"dvz")), Value(0.0)));

  const double weight = this->weight();
  o_dvy = o_dvy / weight;
  const controller::BaseController* ctrl = emitter.ctrl();
  const double ud =
      entity::is_fighter_data(emitter.data()) && ctrl != nullptr ? static_cast<double>(ctrl->UD())
                                                                : 0.0;

  const Value max_hp = field_or(opoint, u"max_hp");
  if (is_num(max_hp)) {
    const double v = to_number(max_hp);
    set_hp_max(v);
    set_hp_r(v);
    set_hp(v);
  }
  const Value hp_field = field_or(opoint, u"hp");
  if (is_num(hp_field)) {
    const double v = to_number(hp_field);
    set_hp_r(v);
    set_hp(v);
  }
  const Value max_mp = field_or(opoint, u"max_mp");
  if (is_num(max_mp)) {
    const double v = to_number(max_mp);
    set_mp_max(v);
    set_mp(v);
  }
  const Value mp_field = field_or(opoint, u"mp");
  if (is_num(mp_field)) set_mp(to_number(mp_field));

  const double dvy_now = num_of(dvy());
  const double dvz_now = num_of(dvz());
  const double dvx_now = num_of(dvx());
  const Value vxm = frame.vxm();
  const Value vym = frame.vym();
  const Value vzm = frame.vzm();
  const double acc_x = std::isnan(frame.acc_x()) ? 0.0 : frame.acc_x();
  const double acc_y = std::isnan(frame.acc_y()) ? 0.0 : frame.acc_y();
  const double acc_z = std::isnan(frame.acc_z()) ? 0.0 : frame.acc_z();

  const Value result_state =
      result.has_value() && result->frame != nullptr ? Value(result->frame->state()) : Value();
  const bool z_disabled = equals(result_state, Value(static_cast<double>(StateEnum::Normal))) ||
                          equals(result_state, Value(static_cast<double>(StateEnum::Burning)));
  const bool z_launch =
      equals(result_state, Value(static_cast<double>(StateEnum::Ball_Flying))) ||
      equals(result_state, Value(static_cast<double>(StateEnum::Ball_3006))) ||
      equals(result_state, Value(static_cast<double>(StateEnum::Weapon_Throwing))) ||
      equals(result_state, Value(static_cast<double>(StateEnum::HeavyWeapon_InTheSky)));

  const Value ovx = Value(offset_velocity.x);
  const Value ovy = Value(offset_velocity.y);
  const Value ovz = Value(offset_velocity.z);
  if (o_dvx > 0) o_dvx = o_dvx / weight - std::abs(offset_velocity.z / 2);
  else o_dvx = o_dvx / weight + std::abs(offset_velocity.z / 2);
  double vx = to_number(ovx) + o_dvx * facing_value;
  double vy = to_number(ovy) + o_dvy + dvy_now;
  const double o_speedz =
      has_speedz ? speedz_explicit : (z_launch ? defines::num(u"Defines.DEFAULT_OPOINT_SPEED_Z") : 0.0);
  double vz = z_disabled ? 0 : to_number(ovz) + o_dvz + o_speedz * ud;
  if (equals(vxm, Value(static_cast<double>(SpeedMode::Fixed)))) vx = dvx_now;
  if (equals(vym, Value(static_cast<double>(SpeedMode::Fixed)))) vy = dvy_now;
  if (equals(vzm, Value(static_cast<double>(SpeedMode::Fixed)))) vz = dvz_now;
  if (equals(vxm, Value(static_cast<double>(SpeedMode::Extra))) && acc_x != 0) vx += acc_x;
  if (equals(vym, Value(static_cast<double>(SpeedMode::Extra))) && acc_y != 0) vy += acc_y;
  if (equals(vzm, Value(static_cast<double>(SpeedMode::Extra))) && acc_z != 0) vz += acc_z;

  prev_velocity.x = velocity.x = round_float(vx);
  prev_velocity.y = velocity.y = round_float(vy);
  prev_velocity.z = velocity.z = round_float(vz);

  if (equals(field_or(opoint, u"kind"), Value(static_cast<double>(OpointKind::Pick)))) {
    emitter.drop_holding();
    bearer = &emitter;
    bearer->holding = this;
  }
  motionless = to_number(or_nullish(field_or(opoint, u"motionless"), Value(2.0)));
  return *this;
}

Entity& Entity::attach(const Value& ghost) {
  if (truthy(Value(_mounted))) return *this;
  _spawn_time = world().game_time();
  _mounted = 1;
  _ghosted = truthy(ghost) ? 1 : 0;
  if (truthy(Value(_ghosted))) {
    motionless = 0;
    shaking = 0;
  }
  world().add_entities(*this);

  set_state(frame.state());

  set_position(Value(position.x), Value(position.y), Value(position.z));
  if (position.y > ground_y()) {
    leave_ground();
  } else {
    is_on_ground = true;
  }
  if (strict_equals(frame.id(), Value(std::u16string(frame_id::kNone)))) {
    enter_frame(auto_frame_value());
  }
  return *this;
}

void Entity::apply_opoints(const std::vector<Value>& opoints) {
  std::vector<Value> copy = opoints;
  apply_opoints(Value(std::make_shared<Array>(std::move(copy))));
}

void Entity::apply_opoints(const Value& opoints_value) {
  apply_opoints(read_list<Opoint>(opoints_value));
}

void Entity::apply_opoints(const std::vector<Opoint>& list) {
  for (const Opoint& item : list) {
    const Value& opoint = item.to_value();
    const double interval = item.interval();
    const Value& interval_id = item.interval_id();
    const bool interval_mode_1 = item.interval_mode() == 1.0;

    std::size_t found = opoints.size();
    for (std::size_t i = 0; i < opoints.size(); ++i) {
      if (strict_equals(field_or(opoints[i].first, u"interval_id"), interval_id)) {
        found = i;
        break;
      }
    }
    if (found != opoints.size() && interval_mode_1) {
      if (!(opoints[found].second == interval)) continue;
    } else if (interval > 0) {
      opoints.push_back({opoint, 0});
    }

    std::vector<Entity*> enemies;
    std::vector<Entity*> allies;
    Value multi_type = Value();
    double count = 0;
    const Value& multi_field = item.multi();
    const Value multi =
        std::holds_alternative<std::monostate>(multi_field) ? Value(1.0) : multi_field;
    if (is_num(multi)) {
      count = to_number(multi);
    } else if (truthy(multi)) {
      const Value type = field_or(multi, u"type");
      const Value min_field = field_or(multi, u"min");
      const Value min =
          std::holds_alternative<std::monostate>(min_field) ? Value(0.0) : min_field;
      const Value max_field = field_or(multi, u"max");
      const Value max =
          std::holds_alternative<std::monostate>(max_field) ? Value(355.0) : max_field;
      const Value skip_zero = field_or(multi, u"skip_zero");
      multi_type = type;
      if (strict_equals(type, Value(static_cast<double>(OpointMultiEnum::AccordingEnemies)))) {
        enemies = world().list_entities(u"ef_" + team(), [this](Entity& o) {
          return entity::is_fighter_data(o.data()) && team() != o.team() && o.hp() > 0;
        });
        if (!(truthy(skip_zero) && enemies.empty())) {
          count = clamp(static_cast<double>(enemies.size()), to_number(min), to_number(max));
        }
      } else if (strict_equals(type,
                               Value(static_cast<double>(OpointMultiEnum::AccordingAllies)))) {
        allies = world().list_entities(u"af_" + team(), [this](Entity& o) {
          if (!entity::is_fighter_data(o.data())) return false;
          if (team() != o.team()) return false;
          if (!(o.hp() > 0)) return false;
          if (&o == this) return false;
          const std::u16string* src = src_emitter();
          if (src != nullptr && o.id == *src) return false;
          return true;
        });
        if (!(truthy(skip_zero) && allies.empty())) {
          count = clamp(static_cast<double>(allies.size()), to_number(min), to_number(max));
        }
      } else if (strict_equals(type, Value(static_cast<double>(OpointMultiEnum::Emitter)))) {
        const std::u16string* emitter_id = emitter();
        if (emitter_id != nullptr && !emitter_id->empty()) {
          Entity* target = world().find_entity(*emitter_id);
          if (target != nullptr) {
            allies.push_back(target);
            count = 1;
          }
        }
      }
    }

    double facing_now = facing;
    for (double i = 0; i < count; ++i) {
      Vector3 sp;
      const double spreading_num = item.spreading();
      const bool has_spreading = !std::isnan(spreading_num);
      if (!has_spreading ||
          spreading_num == static_cast<double>(OpointSpreading::Normal)) {
        sp.z = (i - (count - 1) / 2) * 2.5;
      } else if (spreading_num == static_cast<double>(OpointSpreading::Spreading)) {
        sp.x = num_of(gen_or(opoint, u"__gen_spread_x", Value(sp.x)));
        sp.y = num_of(gen_or(opoint, u"__gen_spread_y", Value(sp.y)));
        sp.z = num_of(gen_or(opoint, u"__gen_spread_z", Value(sp.z)));
        facing_now = sp.x < 0 ? -1 : sp.x > 0 ? 1 : facing_now;
      }
      Entity* e = spawn(opoint, sp, facing_now);
      if (e == nullptr) return;

      if (spreading_num == static_cast<double>(OpointSpreading::FloatRange)) {
        Value x = Value(e->velocity.x);
        Value y = Value(e->velocity.y);
        Value z = Value(e->velocity.z);
        const Value gx = gen_or(opoint, u"__gen_spread_x", x);
        const Value gy = gen_or(opoint, u"__gen_spread_y", y);
        const Value gz = gen_or(opoint, u"__gen_spread_z", z);
        if (!nullish(gx)) x = gx;
        if (!nullish(gy)) y = gy;
        if (!nullish(gz)) z = gz;
        e->set_velocity(x, y, z);
      }

      controller::BaseController* ctrl = e->ctrl();
      if (ctrl != nullptr && ctrl->is_ball_ctrl()) {
        if (strict_equals(multi_type,
                          Value(static_cast<double>(OpointMultiEnum::AccordingEnemies)))) {
          ctrl->chasing = enemies.empty()
                              ? nullptr
                              : enemies[static_cast<std::size_t>(std::fmod(i, enemies.size()))];
        } else if (strict_equals(
                       multi_type,
                       Value(static_cast<double>(OpointMultiEnum::AccordingAllies)))) {
          ctrl->chasing = allies.empty()
                              ? nullptr
                              : allies[static_cast<std::size_t>(std::fmod(i, allies.size()))];
        } else if (strict_equals(multi_type,
                                 Value(static_cast<double>(OpointMultiEnum::Emitter)))) {
          ctrl->chasing = allies.empty() ? nullptr : allies[0];
        }
      }

      const Value inherit_x = Value(item.inherit_speed_x());
      const Value inherit_y = Value(item.inherit_speed_y());
      const Value inherit_z = Value(item.inherit_speed_z());
      const Value vx = truthy(inherit_x)
                           ? Value(e->velocity.x + velocity.x * to_number(inherit_x))
                           : Value(NullTag{});
      const Value vy = truthy(inherit_y)
                           ? Value(e->velocity.y + velocity.y * to_number(inherit_y))
                           : Value(NullTag{});
      const Value vz = truthy(inherit_z)
                           ? Value(e->velocity.z + velocity.z * to_number(inherit_z))
                           : Value(NullTag{});
      e->set_velocity(vx, vy, vz);
    }
  }
}

void Entity::update() {
  if ((*world().lfw().mt()).debugging) {
    (*world().lfw().mt()).log_case({Value(u"e_" + id + u"_" + to_string(name()) + u"_start")});
  }
  _atom_time = num_of(dataset(u"atom_time"));
  _lifetime += _atom_time;
  const Value frame_facing = frame.facing();
  if (truthy(frame_facing)) facing = handle_facing_flag(frame_facing);
  sync_ctrl();
  if (check_fusion_dismissing()) return;
  hp_recovering();
  mp_recovering();

  const Value frame_hp = frame.hp();
  if (truthy(frame_hp)) set_hp(hp() - to_number(frame_hp) * _atom_time);
  const Value frame_mp = frame.mp();
  if (truthy(frame_mp)) set_mp(mp() - to_number(frame_mp) * _atom_time);

  if (!(shaking > 0) || equals(Value(0.0), dataset(u"vrest_after_shaking"))) {
    for (auto it = vrests.begin(); it != vrests.end();) {
      collision::Collision& v = it->second;
      if (v.rest > 0) {
        v.rest = round_float(v.rest - _atom_time);
        if (v.rest < 0) v.rest = 0;
        ++it;
      } else {
        const std::u16string key = it->first;
        del_v_rest(key);
        it = vrests.upper_bound(key);
      }
    }
  }

  if (equals(Value(0.0), dataset(u"arest_after_motionless")) || !(motionless > 0)) {
    if (arest() > 0) {
      set_arest(round_float(arest() - _atom_time));
      if (arest() < 0) set_arest(0);
    } else {
      set_arest(0);
    }
  }

  if (_invisible > 0) {
    _invisible = round_float(_invisible - _atom_time);
    if (_invisible <= 0) _invisible = 0;
  }
  if (_invulnerable > 0) {
    _invulnerable = round_float(_invulnerable - _atom_time);
    if (_invulnerable < 0) _invulnerable = 0;
  }
  if (_blinking > 0) {
    _blinking = round_float(_blinking - _atom_time);
    if (_blinking <= 0) {
      _blinking = 0;
      if (_after_blink.has_value() && *_after_blink == frame_id::kGone) {
        frame = gone_frame_info();
        set_arest(0);
      } else if (_after_blink.has_value() && *_after_blink == frame_id::kRespawn) {
        set_hp(hp_max());
        set_hp_r(hp_max());
        double max_distance = kMaxSafeInteger;
        Entity* friend_entity = nullptr;
        for (Entity* e : world().puppets_values()) {
          if (e == nullptr || !(e->hp() > 0)) continue;
          const double d = abs(round(e->position.x - position.x)) +
                           abs(round(e->position.z - position.z));
          if (d > max_distance) continue;
          max_distance = d;
          friend_entity = e;
        }
        if (friend_entity != nullptr) {
          (*world().lfw().mt()).mark = u"u_1";
          const double x = (*world().lfw().mt()).range(
              max(round(friend_entity->position.x - 100),
                  to_number(world().stage_value(u"player_l"))),
              min(round(friend_entity->position.x + 100),
                  to_number(world().stage_value(u"player_r"))));
          (*world().lfw().mt()).mark = u"u_2";
          const double z = (*world().lfw().mt()).range(
              min(round(friend_entity->position.z - 100),
                  to_number(world().stage_value(u"far"))),
              max(round(friend_entity->position.z + 100),
                  to_number(world().stage_value(u"near"))));
          set_position(Value(x), Value(300.0), Value(z));
        } else {
          set_position(Value(NullTag{}), Value(300.0), Value());
        }
        const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
        enter_frame(auto_frame != nullptr ? *auto_frame : Value());
      }
    }
  }

  for (std::size_t i = 0; i < opoints.size(); ++i) {
    const Value& opoint = opoints[i].first;
    if (strict_equals(Value(opoints[i].second), field_or(opoint, u"interval"))) {
      apply_opoints(Value(std::make_shared<Array>(std::vector<Value>{opoint})));
      opoints[i].second = 0;
    } else {
      opoints[i].second = opoints[i].second + 1;
    }
  }

  stat_recovering();
  toughness_recovering();

  if (_state != nullptr && _state->pre_update) _state->pre_update(*this);
  _from_wait_block = true;
  if (wait > 0) {
    if (!(motionless > 0) && !(shaking > 0) && catcher == nullptr && bearer == nullptr) {
      set_motionless_ticks(0);
      wait = round_float(wait - _atom_time);
      if (wait < 0) wait = 0;
    } else if (motionless > 0 && catcher == nullptr && bearer == nullptr) {
      set_motionless_ticks(round_float(motionless_ticks() + _atom_time));
      if (motionless_ticks() >= kMotionlessWaitTicks) {
        set_motionless_ticks(0);
        wait = round_float(wait - _atom_time);
        if (wait < 0) wait = 0;
      }
    }
  } else if (frame.next()) {
    enter_frame(frame.next());
  } else {
    set_frame(*find_auto_frame());
  }
  _from_wait_block = false;

  const double tick_atom_time = _atom_time;
  const double sub_steps = is_integer(tick_atom_time) && tick_atom_time > 1 && tick_atom_time <= 8
                               ? tick_atom_time
                               : 1.0;
  if (sub_steps > 1) _atom_time = round_float(tick_atom_time / sub_steps);
  for (double i = 0; i < sub_steps; ++i) {
    handle_gravity();
    update_velocity(frame);
    if (i == 0 && _state != nullptr) _state->update(*this);
    update_position();
  }
  _atom_time = tick_atom_time;

  if (motionless > 0) {
    motionless = round_float(motionless - _atom_time);
    if (motionless < 0) motionless = 0;
  }
  if (shaking > 0) {
    shaking = round_float(shaking - _atom_time);
    if (shaking < 0) shaking = 0;
  }

  if (update_catching()) return;
  if (update_caught()) return;

  if (ctrl_ != nullptr) {
    sync_ctrl();
    const controller::ControllerResult& res = ctrl_->update();
    if (const ResolvedNextFrame* const next = res.resolved(); next != nullptr) {
      const EnterFrameResult r = handle_next_frame_result(*next);
      if (static_cast<int>(r) >= static_cast<int>(EnterFrameResult::Entered) &&
          res.keys() != std::u16string(gk::ka)) {
        set_catch_time(catch_time_max());
      }
    }
  }
  sync_ctrl();

  if (!truthy(Value(shaking)) && !truthy(Value(motionless)) && bearer == nullptr &&
      catcher == nullptr) {
    update_landable();
  }
  if (holding != nullptr) holding->follow_bearer();
  collision_list.clear();
  collided_list.clear();
  prev_position = position;
  update_aabb();
  if ((*world().lfw().mt()).debugging) {
    (*world().lfw().mt()).log_case({Value(u"e_" + id + u"_" + to_string(name()) + u"_end")});
  }
}

void Entity::update_ghost() {
  _atom_time = num_of(dataset(u"atom_time"));
  _lifetime += _atom_time;
  const Value frame_facing = frame.facing();
  if (truthy(frame_facing)) facing = handle_facing_flag(frame_facing);
  const Value frame_hp = frame.hp();
  if (truthy(frame_hp)) set_hp(hp() - to_number(frame_hp) * _atom_time);
  const Value frame_mp = frame.mp();
  if (truthy(frame_mp)) set_mp(mp() - to_number(frame_mp) * _atom_time);
  if (_invisible > 0) {
    _invisible = round_float(_invisible - _atom_time);
    if (_invisible <= 0) _invisible = 0;
  }
  if (_blinking > 0) {
    _blinking = round_float(_blinking - _atom_time);
    if (_blinking <= 0) _blinking = 0;
  }

  if (_state != nullptr && _state->pre_update) _state->pre_update(*this);
  _from_wait_block = true;
  if (wait > 0) {
    wait = round_float(wait - _atom_time);
    if (wait < 0) wait = 0;
  } else if (frame.next()) {
    enter_frame(frame.next());
  } else {
    set_frame(*find_auto_frame());
  }
  _from_wait_block = false;

  const double tick_atom_time = _atom_time;
  const double sub_steps = is_integer(tick_atom_time) && tick_atom_time > 1 && tick_atom_time <= 8
                               ? tick_atom_time
                               : 1.0;
  if (sub_steps > 1) _atom_time = round_float(tick_atom_time / sub_steps);
  for (double i = 0; i < sub_steps; ++i) {
    handle_gravity();
    update_velocity(frame);
    if (i == 0 && _state != nullptr) _state->update(*this);
    update_position();
  }
  _atom_time = tick_atom_time;

  if (bearer == nullptr && catcher == nullptr) update_landable();
  prev_position = position;
}

bool Entity::check_fusion_dismissing() {
  if (fuse_bys.empty()) return false;

  const double x = position.x;
  const double y = position.y;
  const double z = position.z;
  for (Entity* fighter : fuse_bys) {
    if (fighter == nullptr) continue;
    fighter->position.set(x, y, z);
  }
  if (dismiss_time.has_value()) {
    dismiss_time = round_float(*dismiss_time - _atom_time);
  }

  const bool should_dismiss =
      ((dismiss_time.has_value() && *dismiss_time <= 0) ||
       (ctrl_ != nullptr && ctrl_->sametime_keys_test(u"dja")) ||
       (ctrl_ != nullptr && ctrl_->sequence_keys_test(u"ja"))) &&
      y == 0;
  if (should_dismiss) dismiss_fusion(u"112");
  return should_dismiss;
}

void Entity::dismiss_fusion(const std::u16string& frame_id) {
  if (fuse_bys.empty()) return;
  const double size = static_cast<double>(fuse_bys.size() + 1);
  const double hp_v = round(hp() / size);
  const double hp_r_v = round(hp_r() / size);
  const double mp_v = round(mp() / size);
  double f = facing;
  set_hp(hp_v);
  set_mp(mp_v);
  set_hp_r(hp_r_v);
  const std::vector<Entity*> members = fuse_bys;
  for (Entity* fighter : members) {
    if (fighter == nullptr) continue;
    fighter->set_hp(hp_v);
    fighter->set_mp(mp_v);
    fighter->set_hp_r(hp_r_v);
    f = to_number(entity::turn_face(Value(f)));
    fighter->facing = f;
    fighter->enter_frame_by_id(Value(frame_id), true);
    fighter->set_invisible(0);
    fighter->motionless = 0;
    fighter->set_invulnerable(0);
  }
  if (dismiss_data.has_value()) transform(*dismiss_data);
  enter_frame_by_id(Value(frame_id), true);
  dismiss_time = std::nullopt;
  dismiss_data.reset();
  fuse_bys.clear();
  has_fuse_bys = false;
}

void Entity::update_aabb() {
  const double bx1 = std::isnan(frame.aabb_x1()) ? 0.0 : frame.aabb_x1();
  const double fx1 = std::isnan(frame.aabb_x2()) ? 0.0 : frame.aabb_x2();
  const double bz1 = std::isnan(frame.aabb_z1()) ? -12.0 : frame.aabb_z1();
  const double bz2 = std::isnan(frame.aabb_z2()) ? 12.0 : frame.aabb_z2();
  const double width = to_number(frame.width());
  const double centerx = to_number(frame.centerx());
  aabb_min_x = round(position.x + (facing > 0 ? bx1 : -fx1));
  aabb_max_x = round(position.x + (facing > 0 ? fx1 : -bx1));
  aabb_min_z = round(position.z + bz1);
  aabb_max_z = round(position.z + bz2);
  l_len = facing > 0 ? centerx : width - centerx;
  r_len = facing > 0 ? width - centerx : centerx;
}

void Entity::update_landable() {
  const double ground = _ground_y;
  const bool was_on_ground = is_on_ground;

  if (!frame.hit_ground_bdys().empty()) update_itr_bdy_hit_ground(frame.hit_ground_bdys());
  if (!frame.hit_ground_itrs().empty()) update_itr_bdy_hit_ground(frame.hit_ground_itrs());

  if (!truthy(frame.landable())) return;

  const bool just_land = !was_on_ground && (position.y <= ground);
  if (just_land) {
    is_on_ground = true;
    position.y = ground;
    _temp_v.x = velocity.x;
    _temp_v.y = velocity.y;
    _temp_v.z = velocity.z;
    velocity.y = 0;
    prev_velocity.y = 0;
    if (_state != nullptr && _state->on_landing) {
      _state->on_landing(*this, position_value(_temp_v));
    }
    play_sound(field_or(_data.base(), u"drop_sounds"));
    if (truthy(Value(throwinjury))) {
      set_hp(hp() - throwinjury);
      set_hp_r(hp_r() - round(throwinjury * (1 - num_of(dataset(u"hp_recoverability")))));
      throwinjury = 0;
    }
    if (truthy(Value(fallinjury))) {
      set_hp(hp() - fallinjury);
      set_hp_r(hp_r() - round(fallinjury * (1 - num_of(dataset(u"hp_recoverability")))));
      fallinjury = 0;
    }
    _landing_frame = frame;
  } else if (was_on_ground) {
    if (position.y - ground > world().ground.step()) {
      leave_ground();
      if (_state != nullptr && _state->on_leave_ground) _state->on_leave_ground(*this);
    } else {
      position.y = ground;
    }
  }
}

bool Entity::update_catching() {
  Entity* caught = catching;
  if (caught == nullptr) return false;
  if (!truthy(Value(_catch_time))) {
    set_catching(nullptr);
    const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
    enter_frame(auto_frame != nullptr ? *auto_frame : Value());
    return true;
  }
  const Cpoint& cpoint_a = frame.cpoint();
  if (!cpoint_a.valid()) {
    set_catching(nullptr);
    const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
    enter_frame(auto_frame != nullptr ? *auto_frame : Value());
    return true;
  }

  const Value tix = Value(cpoint_a.throwvx());
  const Value tiy = Value(cpoint_a.throwvy());
  const Value tiz = Value(cpoint_a.throwvz());
  const double cp_throwinjury = (std::isnan(cpoint_a.throwinjury()) ? 0.0 : cpoint_a.throwinjury());
  const double decrease = (std::isnan(cpoint_a.decrease()) ? 0.0 : cpoint_a.decrease());

  if (truthy(Value(decrease))) add_catch_time(decrease * _atom_time);

  if (cp_throwinjury < -1) {
    const Value* gone = defines::find(u"Defines.NEXT_FRAME_GONE");
    enter_frame(gone != nullptr ? *gone : Value());
    return true;
  }
  if (cp_throwinjury == -1 && (!is_survival_rank_mode(world().lfw().game_mode()) ||
                               !entity::is_boss(*caught))) {
    transfrom_to_another(caught->_data);
    drop_catching();
    caught->catcher = nullptr;
    caught->_prev_cpoint_a = Value(NullTag{});
    caught->enter_frame(caught->get_caught_end_frame());
    return true;
  }
  if (truthy(tix) || truthy(tiy) || truthy(tiz)) {
    set_catching(nullptr);
    return false;
  }

  caught->follow_catcher();
  return false;
}

bool Entity::update_caught() {
  Entity* cer = catcher;
  if (cer == nullptr) return false;
  follow_catcher();
  if (!truthy(Value(cer->_catch_time))) {
    catcher = nullptr;
    _prev_cpoint_a = Value(NullTag{});
    enter_frame(get_caught_end_frame());
    return true;
  }

  const Cpoint& cp_a = cer->frame.cpoint();
  if (!cp_a.valid()) {
    catcher = nullptr;
    _prev_cpoint_a = Value(NullTag{});
    set_velocity(Value(NullTag{}), Value(3.0), Value());
    if (position.y <= ground_y()) position.y = ground_y() + 1;
    const Value* auto_frame = defines::find(u"Defines.NEXT_FRAME_AUTO");
    enter_frame(auto_frame != nullptr ? *auto_frame : Value());
    return true;
  }

  if (!strict_equals(_prev_cpoint_a, cp_a.to_value())) {
    const Value injury = Value(cp_a.injury());
    if (truthy(injury)) {
      const double prev_hp = hp();
      set_hp(hp() - to_number(injury));
      set_hp_r(hp_r() - to_number(injury) * (1 - num_of(dataset(u"hp_recoverability"))));
      summary_mgr().apply_damage(*cer, injury, *this, Value(prev_hp));
    }
    const Value cp_shaking = Value(cp_a.shaking());
    const Value cp_motionless = Value(cp_a.motionless());
    if (truthy(cp_shaking)) shaking = max(to_number(cp_shaking), shaking);
    if (truthy(cp_motionless)) cer->motionless = max(to_number(cp_motionless), cer->motionless);
  }
  _prev_cpoint_a = cp_a.to_value();

  const double ti = (std::isnan(cp_a.throwinjury()) ? 0.0 : cp_a.throwinjury());
  if (ti > 0) throwinjury = ti;
  const Value tx = Value(cp_a.throwvx());
  const Value ty = Value(cp_a.throwvy());
  const Value tz = Value(cp_a.throwvz());
  if (truthy(tx) || truthy(ty) || truthy(tz)) {
    follow_catcher();
    catcher = nullptr;
    _prev_cpoint_a = Value(NullTag{});
  }
  const Value vaction = Value(cp_a.vaction());
  if (truthy(vaction)) {
    return static_cast<int>(enter_frame(vaction)) >= static_cast<int>(EnterFrameResult::Entered);
  }
  return false;
}

void Entity::sync_ctrl() {
  if (ctrl_ == nullptr) return;
  ctrl_->set_entity(this);
  ctrl_->result.set_resolver([this](const Value& nf) { return get_next_frame(nf); });}


namespace {

bool entity_frame_missing(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

}

double Entity::frame_centery() const {
  const double v = frame.centery();
  return std::isnan(v) ? 0.0 : v;
}

double Entity::frame_height() const {
  const double v = frame.height();
  return std::isnan(v) ? 0.0 : v;
}

double Entity::frame_pic_h() const {
  const double v = frame.pic().h();
  return std::isnan(v) ? 0.0 : v;
}

Value Entity::data_indexes_of(const char16_t* key) const {
  return field_or(_data.indexes(), key);
}

Value Entity::data_type() const { return Value(_data.type()); }

namespace {

Entity* common_creator(World* world, const EntityData& data, state::States* states) {
  Entity* ret = world->lfw().acquire_entity(Value(data.type()));
  if (ret == nullptr) return new Entity(*world, data, states);
  ret->reset(data, states);
  return ret;
}

}

void regist_entities() {
  Factory::register_entity(Value(static_cast<double>(EntityEnum::Ball)), common_creator);
  Factory::register_entity(Value(static_cast<double>(EntityEnum::Weapon)), common_creator);
  Factory::register_entity(Value(static_cast<double>(EntityEnum::Entity)), common_creator);
  Factory::register_entity(Value(static_cast<double>(EntityEnum::Fighter)), common_creator);
}

namespace {

const bool s_regist_entities = (regist_entities(), true);

}

}
