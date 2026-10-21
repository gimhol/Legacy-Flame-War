#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/base/fsm.h"
#include "lfw/base/no_emit_callbacks.h"
#include "lfw/bg/background.h"
#include "lfw/core/value.h"
#include "lfw/data/stage_data.h"
#include "lfw/stage/expressions.h"
#include "lfw/stage/item.h"

namespace lfw {

class MersenneTwister;
class World;

class LFW;

namespace stage {

class Stage;

struct StageCallbackArgs {
  Stage* stage = nullptr;
  Value a;
  Value b;
};
using StageCallbacks = CallbacksT<StageCallbackArgs>;

class Stage {
 public:
  static constexpr const char* TAG = "Stage";
  using Warn = std::function<void(const std::u16string&, const std::u16string&)>;

  static void set_warn(Warn warn);
  static void warn(const std::u16string& where, const std::u16string& text);

  Stage(World* world, LFW* lfw, const StageData& data);
  virtual ~Stage();

  World* world() const { return _world; }
  LFW* lfw() const { return _lfw; }
  const StageData& data() const { return _data; }
  const Value& next_stage() const { return _next_stage; }
  StageCallbacks& callbacks() { return _callbacks; }
  FSM& fsm() { return _fsm; }

  virtual double far_plane() const { return _far; }
  virtual double near_plane() const { return _near; }
  void assign_number(const std::u16string& key, double v);
  virtual Value team() const { return Value(_team); }
  virtual bool all_boss_dead();
  virtual Value difficulty() const;
  virtual MersenneTwister* mt();
  virtual Value datas_find(const Value& oid);
  virtual std::shared_ptr<Randoming> datas_randoming_by_group(const Value& oid);
  virtual Entity* create_entity_with_bot(const Value& data);

  Value title() const;
  bool is_stage_finish() const { return _is_stage_finish; }
  bool is_chapter_finish() const { return _is_chapter_finish; }
  Background* bg() const;
  const std::vector<StagePhaseInfo>& phases() const;
  std::u16string id() const;
  std::u16string name() const;
  double time() const { return _fsm.time(); }
  void set_time(double v) { _fsm.set_time(v); }
  double phase_idx() const { return _phase_idx; }
  const Value& phase() const { return _phase; }
  double dialog_idx() const { return _dialogs.index; }
  const DialogInfo* dialog() const;
  bool world_pause() const;
  bool control_disabled() const;
  bool weapon_rain_disabled() const;

  Background* change_bg(const Value& data);
  void stop_bgm();
  void push_dialogs(const std::vector<DialogInfo>& more);
  void next_dialog();
  void clear_dialogs();
  void enter_phase(double idx);
  double ce();
  void spawn(const StagePhaseInfo& phase, const StageObjectInfo& obj, double count);
  void kill_all();
  void kill_soliders();
  void kill_boss();
  void kill_others();
  void dispose();
  bool all_fighter_dead();
  bool dialog_cleared() const;
  bool is_phase_end();
  bool is_dialog_end();
  bool check_phase_end();
  bool check_dialog_end();
  bool should_goto_next_stage();
  void update();

  std::vector<std::unique_ptr<Item>> items;
  Expressions<Stage> dialog_end_tester;
  Expressions<Stage> phase_end_tester;
  double phase_time = 0.0;
  double dialog_time = 0.0;
  double left = 0.0;
  double right = 0.0;
  double width = 0.0;
  double depth = 0.0;
  Background::Middle middle;
  double player_l = 0.0;
  double player_r = 0.0;
  double cam_l = 0.0;
  double cam_r = 0.0;
  double enemy_l = 0.0;
  double enemy_r = 0.0;
  double drink_l = 0.0;
  double drink_r = 0.0;

 private:
  struct DialogState {
    double index = -1.0;
    std::vector<DialogInfo> list;
  };

  void set_phase(const StagePhaseInfo* phase);
  void play_phase_sounds();
  void apply_phase_bounds();
  Value dialog_state_value(const DialogState& s) const;

  World* _world = nullptr;
  LFW* _lfw = nullptr;
  StageData _data;
  Value _next_stage;
  std::u16string _team;
  StageCallbacks _callbacks;
  std::vector<std::function<void()>> _disposers;
  double _phase_idx = 0.0;
  Value _phase;
  StagePhaseInfo _phase_info;
  bool _is_stage_finish = false;
  bool _is_chapter_finish = false;
  DialogState _dialogs;
  std::function<void()> _stop_bgm;
  FSM _fsm;
  std::vector<std::unique_ptr<IState>> _states;
  std::vector<Item*> _released_items;
  double _near = 0.0;
  double _far = 0.0;
};

}
}
