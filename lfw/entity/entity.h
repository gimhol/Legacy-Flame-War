#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/collision/collision.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/data/entity_data.h"
#include "lfw/data/armor_info.h"
#include "lfw/data/frame_info.h"
#include "lfw/defines/i_terrain_info.h"
#include "lfw/defines/i_vector3.h"
#include "lfw/entity/drink_info.h"
#include "lfw/entity/enter_frame_result.h"
#include "lfw/utils/math/mersenne_twister.h"
#include "lfw/utils/times.h"

namespace lfw {

namespace buff {
class Buff;
}

namespace state {
class States;
class State_Base;
}

struct IEntityRenderer {
  virtual ~IEntityRenderer() = default;
  virtual void render(double dt, double dfactor) = 0;
};

class World;
class LFW;
class Entity;

struct Jumping {
  double x = 0;
  double y = 0;
  double z = 0;
  double t = 0;
};

class Entity {
 public:
  static constexpr const char* TAG = "Entity";
  static constexpr double kMotionlessWaitTicks = 16;

  Entity(World& world, const EntityData& data);
  Entity(World& world, const EntityData& data, state::States* states);
  virtual ~Entity();

  Entity(const Entity&) = delete;
  Entity& operator=(const Entity&) = delete;

  std::u16string id;
  double wait = 0;
  double variant = 0;
  std::vector<EntityData> transforms;
  double transform_index = 0;
  Vector3 prev_position;
  Vector3 position;
  Vector3 prev_velocity;
  Vector3 velocity;
  std::vector<std::u16string> emitters;
  std::unique_ptr<DrinkInfo> drink;
  std::vector<Entity*> fuse_bys;
  bool has_fuse_bys = false;
  std::vector<std::u16string> copies;
  std::optional<double> dismiss_time;
  std::optional<EntityData> dismiss_data;
  double stat_bar = 0;
  double fallinjury = 0;
  double throwinjury = 0;
  double facing = 1;
  FrameInfo frame;
  Entity* catching = nullptr;
  Entity* catcher = nullptr;
  double aabb_min_x = 0;
  double aabb_max_x = 0;
  double aabb_min_z = 0;
  double aabb_max_z = 0;
  double l_len = 0;
  double r_len = 0;
  Entity* bearer = nullptr;
  Entity* holding = nullptr;
  double motionless = 0;
  double shaking = 0;
  bool bounced = false;
  Value lying_a_count = Value(0.0);
  Value lying_d_count = Value(0.0);
  Value lying_c_count = Value(0.0);
  bool drop_hurted = false;
  bool dropping = false;
  double name_visible = 0;
  double wakeup_invuln = 0;
  double dead_gone = 0;
  Value dead_join = Value(NullTag{});
  double ctrl_visible = 0;
  std::vector<std::pair<Value, double>> opoints;
  std::vector<collision::Collision> collision_list;
  std::vector<collision::Collision> collided_list;
  std::optional<collision::Collision> lastest_collided;
  bool is_on_ground = false;
  std::map<std::u16string, buff::Buff*> buffs;
  std::map<std::u16string, std::u16string> marks;
  IEntityRenderer* renderer = nullptr;
  bool puppet = false;
  Jumping jumping;
  ITerrainInfo terrain;
  ArmorInfo armor;
  Callbacks callbacks;
  std::map<std::u16string, collision::Collision> vrests;
  std::map<std::u16string, collision::Collision> blockers;
  std::map<std::u16string, collision::Collision> superpunchs;

  void reset(const EntityData& data);
  void reset(const EntityData& data, state::States* states);
  void set_state(const Value& state_code);
  state::State_Base* state_ptr() const { return _state; }

  double lifetime() const { return _lifetime; }
  double spawn_time() const { return _spawn_time; }
  double render_effect_time() const { return _render_effect_time; }
  std::u16string outline_color() const;
  virtual void set_outline_color(std::u16string v);
  virtual void set_outline_color_raw(const Value& v) { set_outline_color(to_string(v)); }
  double outline_alpha() const { return _outline_alpha; }
  virtual void set_outline_alpha(double v);
  double outline_width() const { return _outline_width; }
  virtual void set_outline_width(double v);
  Value outline_enabled() const;
  void set_outline_enabled(const Value& v);
  const std::u16string& mix_color() const { return _mix_color; }
  void set_mix_color(std::u16string v);
  double mix_strength() const { return _mix_strength; }
  void set_mix_strength(double v);
  double greyscale() const { return _greyscale; }
  void set_greyscale(double v);

  virtual double ground_y() const { return _ground_y; }
  virtual const EntityData& data() const { return _data; }
  std::u16string origin_data_id() const;
  virtual Value group() const;
  virtual double mounted() const { return _mounted; }
  double ghosted() const { return _ghosted; }
  void set_ghosted(double v) { _ghosted = v; }
  double reserve() const { return _reserve; }
  virtual void set_reserve(double v);
  double type() const;
  const std::vector<ItrInfo>& itr() const;
  const std::vector<BdyInfo>& bdy() const;

  double toughness_resting_max() const { return _toughness_resting_max; }
  void set_toughness_resting_max(double v);
  double resting_max() const;
  void set_resting_max(double v);
  virtual double resting() const { return _resting; }
  virtual void set_resting(double v);
  virtual double fall_value() const { return _fall_value; }
  virtual void set_fall_value(double v);
  virtual double toughness() const { return _toughness; }
  virtual void set_toughness(double v);
  virtual double toughness_max() const { return _toughness_max; }
  void set_toughness_max(double v);
  double toughness_resting() const { return _toughness_resting; }
  void set_toughness_resting(double v);
  virtual double catch_time_max() const;
  void set_catch_time_max(double v);
  virtual double fall_value_max() const;
  void set_fall_value_max(double v);
  virtual double defend_value() const { return _defend_value; }
  virtual void set_defend_value(double v);
  virtual double defend_value_max() const;
  void set_defend_value_max(double v);
  virtual double defend_ratio() const;
  void set_defend_ratio(double v);

  Value name() const;
  virtual void set_name(const Value& v);
  virtual void set_stat_bar(double v) { stat_bar = v; }
  virtual void set_dead_gone(double v) { dead_gone = v; }
  virtual double mp() const { return _mp; }
  virtual void set_mp(double v);
  virtual double hp_r() const { return _hp_r; }
  virtual void set_hp_r(double v);
  virtual double hp() const { return _hp; }
  virtual void set_hp(double v);
  virtual double mp_max() const;
  virtual void set_mp_max(double v);
  virtual double hp_max() const;
  virtual void set_hp_max(double v);
  virtual const std::u16string& team() const { return _team; }
  virtual void set_team(std::u16string v);
  const std::u16string* src_emitter() const;
  const std::u16string* emitter() const;
  double blinking() const { return _blinking; }
  void set_blinking(double v);
  double invisible() const { return _invisible; }
  virtual void set_invisible(double v);
  double invulnerable() const { return _invulnerable; }
  virtual void set_invulnerable(double v);
  Value bot_ignore() const;
  controller::BaseController* ctrl() const { return ctrl_; }
  void set_ctrl(controller::BaseController* v);
  World& world() const { return *world_; }
  LFW& lfw() const;
  void as_key_role(const Value& v);
  void auto_key_role();
  double gravity() const;
  double itr_motionless() const;
  virtual double arest() const { return _arest; }
  virtual void set_arest(double v);
  virtual double weight() const;
  virtual double base_type() const;
  virtual Value state() const;

  virtual double frame_centery() const;
  virtual double frame_height() const;
  virtual double frame_pic_h() const;
  virtual void buffs_set(const std::u16string& key, buff::Buff* b) { buffs[key] = b; }
  virtual void buffs_delete(const std::u16string& key) { buffs.erase(key); }
  Value data_indexes_of(const char16_t* key) const;
  virtual Value data_indexes_falling() const { return data_indexes_of(u"falling"); }
  virtual Value data_indexes_in_the_skys() const { return data_indexes_of(u"in_the_skys"); }
  virtual Value data_type() const;
  virtual void set_fallinjury(const Value& v) { fallinjury = to_number(v); }
  virtual void set_wait(const Value& v) { wait = to_number(v); }
  virtual double velocity_y() const { return velocity.y; }
  virtual Value marks_get(const std::u16string& key) const {
    const auto it = marks.find(key);
    return it == marks.end() ? Value() : Value(it->second);
  }
  virtual void marks_set(const std::u16string& key, const std::u16string& value) {
    marks[key] = value;
  }
  virtual bool marks_delete(const std::u16string& key) { return marks.erase(key) > 0; }

  bool marks_has(const std::u16string& kind) const {
    return marks.find(kind) != marks.end();
  }
  Value data_base_hit_sounds() const;
  virtual Value data_id() const;
  Value frame_id() const;
  Value cpoint_backhurtact() const;
  Value cpoint_fronthurtact() const;
  Value data_in_the_skys_first() const;
  virtual Value holding_base_type() const;
  Value fuse_bys_of() const;
  virtual void set_fuse_bys(const Value& v);
  virtual void set_dismiss_data(const EntityData& v) { dismiss_data = v; }
  virtual void set_dismiss_time(const Value& v);
  virtual void play_sound(const Value& sounds, const Value& pos);
  virtual void play_sound(const Value& sounds);
  virtual void spark_point(const collision::Cube& a, const collision::Cube& b, double& x, double& y,
                           double& z) const;
  virtual void set_prop(const std::u16string& name, const Value& v);
  virtual void set_velocity_x(const Value& v) { velocity.x = to_number(v); }
  virtual void set_facing(const Value& v) { facing = to_number(v); }
  virtual Entity& set_shaking(double v);
  virtual Entity& set_motionless(double v);
  virtual Entity& set_catcher(Entity* v);

  virtual double velocity_x() const { return velocity.x; }
  virtual double velocity_z() const { return velocity.z; }
  virtual Value frame_on_landing() const;
  virtual Value frame_behavior() const;
  virtual const Object& data_base() const { return data().base(); }
  virtual const FrameInfoMap& data_frames() const { return data().frames(); }
  /** 按帧 id 从数据表里查；找不到返回 `nullptr`。 */
  virtual const FrameInfo* data_frame(const std::u16string& id) const {
    return data().find_frame(id);
  }
  virtual bool has_data_indexes() const;
  virtual const FrameInfo& frame_info() const { return frame; }
  virtual Value jumping_x() const { return Value(jumping.x); }
  virtual void set_jumping_x(const Value& v) { jumping.x = to_number(v); }
  virtual Value jumping_y() const { return Value(jumping.y); }
  virtual void set_jumping_y(const Value& v) { jumping.y = to_number(v); }
  virtual Value jumping_z() const { return Value(jumping.z); }
  virtual void set_jumping_z(const Value& v) { jumping.z = to_number(v); }
  virtual Value jumping_t() const { return Value(jumping.t); }
  virtual void set_jumping_t(const Value& v) { jumping.t = to_number(v); }
  virtual void set_bounced(const Value& v) { bounced = truthy(v); }
  virtual void set_throwinjury(const Value& v) { throwinjury = to_number(v); }
  virtual void set_drop_hurted(const Value& v) { drop_hurted = truthy(v); }
  virtual void set_dead_join(const Value& v) { dead_join = v; }
  virtual void set_wakeup_invuln(const Value& v) { wakeup_invuln = to_number(v); }
  virtual void set_lying_a_count(const Value& v) { lying_a_count = v; }
  virtual void set_lying_d_count(const Value& v) { lying_d_count = v; }
  virtual void set_lying_c_count(const Value& v) { lying_c_count = v; }
  void assign_position(double x, double y, double z) {
    position.x = x;
    position.y = y;
    position.z = z;
  }
  virtual bool has_holding() const { return holding != nullptr; }
  virtual DrinkInfo* holding_drink() const;
  virtual void holding_set_hp(const Value& v);
  virtual void holding_set_hp_r(const Value& v);
  virtual void holding_set_velocity(const Value& x, const Value& y, const Value& z);
  virtual void holding_set_team(const Value& v);
  virtual void holding_mt_mark(const std::u16string& mark);
  virtual Value holding_mt_range(double lo, double hi);
  virtual bool holding_is_weapon() const;
  virtual bool has_catcher() const { return catcher != nullptr; }
  virtual void catcher_drop_catching();
  virtual bool has_bearer() const { return bearer != nullptr; }
  virtual Value bearer_motionless() const;
  virtual void set_bearer_motionless(const Value& v);
  virtual void world_etc(double x, double y, double z, const std::u16string& kind) {
    (void)x;
    (void)y;
    (void)z;
    (void)kind;
  }
  virtual void world_callbacks_call(const std::u16string& name) { (void)name; }
  virtual Value ground_segment(double x, double z) {
    (void)x;
    (void)z;
    return Value();
  }
  virtual double ground_y(const Value& segment, double x, double z) {
    (void)segment;
    (void)x;
    (void)z;
    return 0;
  }
  virtual Value datas_find_fighter(const std::u16string& oid);
  virtual double ctrl_ud() const;
  virtual double ctrl_lr() const;
  virtual bool ctrl_is_bot() const;
  virtual bool ctrl_is_end(const std::u16string& key) const;
  virtual void ctrl_reset_key_list();
  virtual Value lfw_new_team() const;
  virtual Value datas_find(const std::u16string& oid);

  bool has_next_frame_judge(const Value& nf) const;
  Value next_frame_judge(const Value& nf);

  Value find_frame_by_id(const Value& id) const;
  virtual Value find_auto_frame() const;
  virtual Value find_align_frame(const std::u16string& frame_id, const Value& src,
                                 const Value& dst) const;
  virtual const FrameInfo& get_prev_frame() const { return _prev_frame; }
  virtual Value get_sudden_death_frame() const;
  virtual Value get_caught_end_frame();
  double handle_facing_flag(const Value& facing) const;
  virtual double handle_wait_flag(const Value& wait,
                                  const std::optional<Value>& frame = std::nullopt) const;
  double get_frame_wait(const Value& frame) const;

  Value dvx() const;
  Value dvy() const;
  Value dvz() const;
  virtual void set_velocity(const Value& x, const Value& y, const Value& z);
  virtual void leave_ground();
  virtual void handle_ground_velocity_decay(double factor = 1);
  virtual void handle_velocity_decay(const Value& accx,
                             std::optional<Value> accz = std::nullopt,
                             double factor = 1);
  void handle_gravity();
  virtual void update_velocity(const FrameInfo& vinfo);
  virtual void set_position(const Value& x, const Value& y, const Value& z);
  void update_position();

  virtual void set_frame(const Value& v);
  virtual void set_frame(const FrameInfo& v) { set_frame(v.to_value()); }
  virtual EnterFrameResult enter_frame(const Value& nf, bool fallback = false);
  virtual EnterFrameResult enter_frame_by_id(const Value& id, bool fallback = false);
  EnterFrameResult handle_next_frame_result(const Value& result, bool fallback = false);
  Value get_next_frame(const Value& which);
  void follow_bearer();
  void follow_catcher();
  virtual void drop_holding();
  void pick(Entity& weapon);
  virtual void transform(const EntityData& data);
  virtual bool transfrom_to_another(const std::optional<EntityData>& data = std::nullopt);

  Entity* spawn(const Value& opoint);
  Entity* spawn(const Value& opoint, const Vector3& offset_velocity, double facing);
  Entity& on_spawn(Entity& emitter, const Value& opoint, const Vector3& offset_velocity,
                   double facing);
  virtual Entity& attach(const Value& ghost = Value(false));
  void release();
  void apply_opoints(const Value& opoints);
  void apply_opoints(const std::vector<Opoint>& opoints);
  void apply_opoints(const std::vector<Value>& opoints);

  void update();
  void update_ghost();
  bool check_fusion_dismissing();
  virtual void dismiss_fusion(const std::u16string& frame_id);
  void update_aabb();
  void update_landable();
  bool update_catching();
  bool update_caught();
  void sync_ctrl();

  void reset_armor();
  virtual Entity& set_catching(Entity* v);
  Entity& add_catch_time(double value);
  virtual Entity& set_catch_time(double value);
  virtual void set_dropping(bool v) { dropping = v; }
  virtual Value dataset(const std::u16string& name) const;
  virtual Value world_dataset(const std::u16string& name) const;
  Value itr_fall(const Value& itr) const;

  void toughness_recovering();
  void fall_value_recovering();
  void defend_value_recovering();
  void stat_recovering();
  void hp_recovering();
  void mp_recovering();

  bool set_mark(const std::u16string& key, const std::u16string& value,
                const std::optional<Value>& prev = std::nullopt);
  bool del_mark(const std::u16string& key,
                const std::optional<Value>& value = std::nullopt);
  bool is_ally(const Entity& other) const;
  Entity* get_emitter(double idx) const;

  void add_v_rest(const collision::Collision& c);
  double get_v_rest(const std::u16string& a_id) const;
  void del_v_rest(const std::u16string& a_id);
  double get_flag(const Entity& other) const;
  void clean_holding();
  void clean_catching();
  bool drop_catching();
  virtual void blink_and_gone(double duration);
  virtual void blink_and_respawn(double duration);
  void update_itr_bdy_hit_ground(const std::vector<ItrInfo>& itrs);
  void update_itr_bdy_hit_ground(const std::vector<BdyInfo>& bdys);

  void to_snapshot(std::vector<Value>& nums, std::vector<std::u16string>& strs) const;
  void read_snapshot(const std::vector<Value>& nums, const std::vector<std::u16string>& strs);
  bool add_copy(const std::u16string& copy_id);

  double catch_time() const { return _catch_time; }
  double toughness_r_value() const { return _toughness_r_value; }
  double fall_r_value() const { return _fall_r_value; }
  double defend_r_value() const { return _defend_r_value; }
  double toughness_r_tick_max() const { return _toughness_r_tick.max(); }
  double hp_r_tick_max() const { return _hp_r_tick.max(); }
  double mp_r_tick_max() const { return _mp_r_tick.max(); }
  double fall_r_tick_max() const { return _fall_r_tick.max(); }
  double defend_r_tick_max() const { return _defend_r_tick.max(); }
  double atom_time() const { return _atom_time; }
  void set_atom_time(double v) { _atom_time = v; }
  const FrameInfo& landing_frame() const { return _landing_frame; }
  void set_landing_frame(const FrameInfo& v) { _landing_frame = v; }
  void set_ground_y(double v) { _ground_y = v; }
  bool from_wait_block() const { return _from_wait_block; }
  void set_from_wait_block(bool v) { _from_wait_block = v; }
  void set_prev_frame(const FrameInfo& v) { _prev_frame = v; }
  const std::optional<std::u16string>& after_blink() const { return _after_blink; }
  double motionless_ticks() const { return _motionless_ticks; }
  void set_motionless_ticks(double v) { _motionless_ticks = v; }
  const Vector3& temp_v() const { return _temp_v; }
  const Value& prev_cpoint_a() const { return _prev_cpoint_a; }
  const Value& next_frame_by_id() const { return _next_frame_by_id; }


 private:
  Value gen_or(const Value& holder, const std::u16string& kind, const Value& fallback) const;

  double _lifetime = 0;
  double _spawn_time = 0;
  double _render_effect_time = 0;
  std::u16string _outline_color;
  double _outline_alpha = 0.8;
  double _outline_width = 1;
  std::optional<double> _outline_enabled;
  std::u16string _mix_color;
  double _mix_strength = 0;
  double _greyscale = 0;
  EntityData _data;
  std::u16string _origin_data_id;  double _reserve = 0;
  double _mounted = 0;
  double _ghosted = 0;
  Times _hp_r_tick;
  Times _mp_r_tick;
  double _resting = 0;
  std::optional<double> _resting_max;
  Times _resting_tick;
  double _toughness = 0;
  double _toughness_max = 0;
  Times _toughness_r_tick;
  double _toughness_r_value = 0;
  double _toughness_resting = 0;
  double _toughness_resting_max = 0;
  double _fall_value = 0;
  std::optional<double> _fall_value_max;
  Times _fall_r_tick;
  double _fall_r_value = 0;
  double _defend_value = 0;
  std::optional<double> _defend_value_max;
  Times _defend_r_tick;
  double _defend_r_value = 0;
  std::optional<double> _defend_ratio;
  Value _name = Value(NullTag{});
  double _mp = 0;
  std::optional<double> _mp_max;
  double _hp = 0;
  double _hp_r = 0;
  std::optional<double> _hp_max;
  double _arest = 0;
  double _motionless_ticks = 0;
  double _catch_time = 0;
  std::optional<double> _catch_time_max;
  double _invisible = 0;
  double _invulnerable = 0;
  double _blinking = 0;
  FrameInfo _landing_frame;
  std::optional<std::u16string> _after_blink;
  double _ground_y = 0;
  double _prev_ground_y = 0;
  double _atom_time = 0;
  bool _from_wait_block = false;
  FrameInfo _prev_frame;
  Value _next_frame_by_id;
  Value _prev_cpoint_a = Value(NullTag{});
  Vector3 _temp_v;
  std::u16string _team;
  state::States* states_ = nullptr;
  state::State_Base* _state = nullptr;
  Entity& mutable_self() const { return const_cast<Entity&>(*this); }
  controller::BaseController* ctrl_ = nullptr;
  World* world_ = nullptr;

};

void regist_entities();

}
