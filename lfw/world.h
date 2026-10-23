#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/fps.h"
#include "lfw/base/no_emit_callbacks.h"
#include "lfw/bg/background.h"
#include "lfw/buff/buff.h"
#include "lfw/camera.h"
#include "lfw/collision/collision.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/defines/i_vector3.h"
#include "lfw/entity/entity.h"
#include "lfw/ground.h"
#include "lfw/player_info.h"
#include "lfw/stage/stage.h"
#include "lfw/transform.h"
#include "lfw/utils/container_help/nested_multi_map.h"
#include "lfw/utils/times.h"
#include "lfw/world_dataset.h"

namespace lfw {

class LFW;

class ITickerOptions;
class Ticker;
class World;
class WorldCollisionHost;

using stage::Stage;

struct WorldCallbackArgs {
  World* world = nullptr;
  Entity* entity = nullptr;
  Stage* stage = nullptr;
  Stage* prev_stage = nullptr;
  Value value = Value(NullTag{});
  Value prev = Value(NullTag{});
  std::u16string key;
  double num = 0.0;
  double num2 = 0.0;
  double num3 = 0.0;
  bool flag = false;
};
using WorldCallbacks = CallbacksT<WorldCallbackArgs>;

class IWorldRenderer {
 public:
  virtual ~IWorldRenderer() = default;
  virtual void add_entity(Entity& e) = 0;
  virtual void del_entity(Entity& e) = 0;
  virtual void render(double dt) = 0;
  virtual void dispose() = 0;
};

class World {
 public:
  static constexpr const char* TAG = "World";

  World(LFW& lfw, IWorldRenderer& renderer, state::States* states);
  virtual ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  WorldDataset dataset;
  WorldCallbacks callbacks;
  std::vector<std::pair<std::u16string, buff::Buff*>> buffs;
  Ground ground;
  Transform transform;
  std::vector<std::pair<std::u16string, Entity*>> entity_map;
  std::vector<Entity*> entities;
  std::vector<Entity*> ghosts;
  std::vector<std::pair<std::u16string, Entity*>> puppets;
  std::vector<std::u16string> puppet_teams;
  std::vector<std::pair<std::u16string, collision::Collision>> collisions;
  double pairs_compared = 0.0;
  double render_cost = 0.0;
  bool has_players_alive = false;
  double TU = 1.0;
  double extra_steps = 0.0;
  double extra_step_budget_ms = 16.0;
  std::vector<std::pair<double, double>> ground_weapon_counts;

  LFW& lfw() const { return *lfw_; }
  IWorldRenderer& renderer() const { return *renderer_; }
  Camera& camera() const { return *camera_; }
  virtual std::vector<Entity*>& entity_list();
  virtual std::vector<Entity*>& puppet_list();
  virtual Value difficulty() const { return Value(dataset.difficulty()); }
  virtual void camera_jump_x(const Value& x);

  virtual Value bg_dataset(const std::u16string& key) const;
  virtual Value stage_value(const std::u16string& key) const;
  virtual Value bg_value(const std::u16string& key) const;
  virtual double entity_count() const;
  virtual std::vector<Entity*> puppets_values();
  virtual std::optional<Value> gen_field(const Value& holder, const std::u16string& kind,
                                        Entity& emitter);
  virtual void play_sound(const Value& sounds, const Value& pos);
  state::States* states() const { return states_; }
  WorldCollisionHost& collision_host();

  virtual Background* bg() const { return bg_.get(); }
  virtual void set_bg(std::unique_ptr<Background> v);
  virtual Stage* stage() const { return stage_.get(); }
  void set_stage(std::unique_ptr<Stage> v);

  double player_l() const;
  double player_r() const;
  double left() const;
  double right() const;
  double near_plane() const;
  double far_plane() const;
  double width() const;
  double depth() const;
  const Background::Middle& middle() const;

  bool paused() const { return paused_ == 1.0; }
  void set_paused(bool v);
  void set_paused_value(double v);
  bool fn_locked() const { return fn_locked_ == 1.0; }
  void set_fn_locked(bool v);
  void set_fn_locked_value(double v);
  const std::vector<std::pair<std::u16string, double>>& counts() const { return counts_; }
  virtual double game_time() const { return game_time_.value(); }
  double lifetime() const { return lifetime_; }
  double paused_value() const { return paused_; }
  double fn_locked_value() const { return fn_locked_; }
  bool sleeping() const { return sleeping_; }
  bool need_fps() const { return need_FPS_; }
  bool need_ups() const { return need_UPS_; }
  const FPS& fps() const { return fps_; }
  int render_worker_id() const { return render_worker_id_; }
  Ticker* ticker() const { return update_worker_.get(); }
  const std::vector<std::pair<std::u16string, double>>& team_alive_counts() const {
    return team_alive_counts_;
  }
  bool stage_limit() const;
  bool world_pause() const;
  bool is_stage_finish() const;
  bool is_chapter_finish() const;

  virtual Value world_stage();
  virtual Value world_bg();
  virtual Value world_dataset();
  Value dataset_value() const;
  World* world_ptr() { return this; }

  void on_dataset_change(const std::u16string& key, const Value& curr, const Value& prev);  void team_come(const std::u16string& team, double x, double y, double z);
  void team_move(const std::u16string& team);
  void team_stay(const std::u16string& team);
  void team_follow(Entity& target);
  virtual void add_entities(Entity& e);
  virtual const std::vector<Entity*>& list_entities(const std::u16string& name,
                                            const std::function<bool(Entity&)>& predicate);
  void del_entity(Entity& e);
  virtual void del_entities(const std::vector<Entity*>& list);
  double fps_value() const;
  void stop_render();
  void start_render();
  void stop_update();
  void sleep();
  void awake();
  double base_step_ms();
  void on_step_error(const std::u16string& message, bool has_errors);
  void step();
  void update_once(double dt);
  void catch_up();
  void start_update();
  std::function<void()> before_update;
  std::function<void()> after_update;
  virtual std::vector<double> get_bound(Entity& e);
  virtual const Vector3& restrict(Entity& e);
  void update_ui();
  void change_bg(const Value& bg_id);
  void change_stage(const Value& stage_id);
  void handle_cmds();
  virtual void mark_players_alive(Entity& e, bool is_alive);
  void collect_dead_buff(buff::Buff& b);
  void add_collision(const collision::Collision& c);
  void render_once(double dt);
  long long ui_render_calls() const;
  long long ui_render_unhandled() const;
  void update_camera();
  void spark(double x, double y, double z, const std::u16string& f);
  void etc(double x, double y, double z, const std::u16string& f);
  collision::Cube get_bounding(Entity& e, const FrameInfo* frame, const Value& info);
  void dispose();
  void add_count(const std::u16string& key, double o);
  void clear();
  void reset_game_time();
  virtual Entity* find_entity(const std::u16string& id) const;
  virtual void buffs_set(const std::u16string& id, buff::Buff* b);
  virtual buff::Buff* buffs_get(const std::u16string& id) const;
  std::optional<collision::Collision> get_collision(const std::u16string& aid,
                                                   const std::u16string& vid) const;
  bool has_collision(const std::u16string& aid, const std::u16string& vid) const;
  std::vector<collision::Collision> get_collisions(const std::u16string& aid,
                                                   const std::u16string& vid,
                                                   std::vector<collision::Collision> out = {}) const;
  double weapon_section_at(double x) const;
  double random_weapon_x(std::optional<double> exclude_section = std::nullopt);
  double weapon_count_at(double x) const;
  void calc_alives();
  std::u16string game_result(bool refresh);

  const std::vector<Entity*>& gones() const { return gones_; }
  size_t alive_players_size() const { return alive_players_.size(); }
  const std::vector<buff::Buff*>& dead_buffs() const { return dead_buffs_; }
  double step_error_count() const { return step_error_count_; }
  void set_step_error_count(double v) { step_error_count_ = v; }

  void set_need_fps(bool v) { need_FPS_ = v; }
  void set_need_ups(bool v) { need_UPS_ = v; }

 private:
  static Entity* map_get(const std::vector<std::pair<std::u16string, Entity*>>& m,
                         const std::u16string& key);
  static void map_set(std::vector<std::pair<std::u16string, Entity*>>& m, std::u16string key,
                      Entity* value);
  static bool vec_has(const std::vector<std::u16string>& v, const std::u16string& key);
  double ui_render_dfactor(double dt);
  void render_ui_layers(double dt);

  LFW* lfw_ = nullptr;
  IWorldRenderer* renderer_ = nullptr;
  state::States* states_ = nullptr;
  std::unique_ptr<Background> bg_;
  std::vector<Entity*> puppet_list_;
  std::unique_ptr<Stage> stage_;
  std::unique_ptr<Camera> camera_;
  std::unique_ptr<WorldCollisionHost> collision_host_;
  bool sleeping_ = false;
  Value spark_data_ = Value(NullTag{});
  Value etc_data_ = Value(NullTag{});
  bool need_FPS_ = true;
  bool need_UPS_ = true;
  FPS fps_{0.9};
  double lifetime_ = 0.0;
  int render_worker_id_ = 0;
  double render_prev_time_ = 0.0;
  double render_fix_radio_ = 1.0;
  double df_utime_ = 0.0;
  double df_dtime_ = 0.0;
  double df_dfactor_ = 1.0;
  std::unique_ptr<Ticker> update_worker_;
  std::unique_ptr<ITickerOptions> update_options_;
  std::map<std::u16string, std::vector<Entity*>> entities_map_;
  std::vector<buff::Buff*> dead_buffs_;
  Times game_time_;
  std::vector<std::pair<std::u16string, double>> counts_;
  std::vector<Entity*> gones_;
  double paused_ = 0.0;
  double fn_locked_ = 0.0;
  std::vector<std::pair<std::u16string, double>> team_alive_counts_;
  std::vector<Entity*> alive_players_;
  NestedMultiMap<std::u16string, std::u16string, collision::Collision> pair_collisions_;
  double step_error_count_ = 0.0;
  double step_error_time_ = 0.0;
  Vector3 restrict_result_;
  std::vector<double> bound_{0.0, 0.0, 0.0, 0.0};
};

}
