#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/controller/controller_double_clicks.h"
#include "lfw/controller/controller_key_status.h"
#include "lfw/controller/controller_result.h"
#include "lfw/controller/key_status.h"
#include "lfw/controller/seq_keys.h"
#include "lfw/core/value.h"
#include "lfw/defines/bin_op.h"
#include "lfw/utils/times.h"

namespace lfw {

class Entity;
class MersenneTwister;
class ICtrlCreator;

namespace controller {

enum class Status { UP = 0, DOWN = 1, HOLD = 2 };

struct CtrlEnv {
  double key_hit_duration = 0;
  double double_click_interval = 0;
  double facing = 1;
  bool alive = true;
  std::u16string team;
  double px = 0;
  double py = 0;
  double pz = 0;
  double frame_state = 0;
  double hp = 0;
  double type = 0;
  Value frame;
  Value pre_hitkeys;
  Value post_hitkeys;
  Value hit;
  Value hld;
  Value kd;
  Value ku;
  Value seq_map;
  Value transform_pre_seq_map;
  Value transform_post_seq_map;
  Value data_pre_seq_map;
  Value data_post_seq_map;
  std::function<Value(const Value&)> get_next_frame;

  Entity* self = nullptr;
  std::function<void(double, double, double, const std::u16string&)> world_etc;
  std::function<void(const std::u16string&, double, double, double)> team_come;
  std::function<void(const std::u16string&)> team_stay;
  std::function<void(const std::u16string&)> team_move;
  std::function<void()> team_follow;

  std::u16string id;
  double hp_max = 0;
  bool mounted = false;
  bool invisible = false;
  bool invulnerable = false;
  double toughness = 0;
  double resting = 0;
  double ground_y = 0;
  bool is_on_ground = false;
  double vx = 0;
  double vy = 0;
  double vz = 0;
  Value name;
  Value data;
  const Entity* holding = nullptr;
  const Entity* catching = nullptr;
  double blockers_count = 0;
  MersenneTwister* mt = nullptr;

  std::function<double()> bot_difficulty;
  std::function<std::vector<double>()> get_bound;
  std::function<bool()> has_players_alive;
  std::function<Value(const std::u16string&)> stage_value;
  std::function<Value(const std::u16string&)> find_bot;
  std::function<Value(const std::u16string&, BinOp op)> entity_val;
  std::function<void(const Value&, const Value&, const Value&)> set_position;
  double bg_width = 0;
  double bg_near = 0;
  double bg_far = 0;
};

class BaseController {
 public:
  static constexpr const char* TAG = "BaseController";

  BaseController();

  virtual void set_env(const CtrlEnv* env) { _env = env; }
  const CtrlEnv* env() const { return _env; }
  void set_creator(const ICtrlCreator* creator) { _creator = creator; }
  const ICtrlCreator* creator() const { return _creator; }
  void set_kind(bool human, bool bot) {
    _is_human = human;
    _is_bot = bot;
  }
  bool is_human() const { return _is_human; }
  bool is_bot() const { return _is_bot; }
  void set_ball(bool v) { _is_ball = v; }
  virtual bool is_ball_ctrl() const { return _is_ball; }
  virtual bool is_invalid_controller() const { return false; }
  Entity* chasing = nullptr;

  double time() const { return _time.value(); }

  int LR() const;
  double RL() const { return -static_cast<double>(LR()); }
  int UD() const;
  double DU() const { return -static_cast<double>(UD()); }
  int jd() const;
  double dj() const { return -static_cast<double>(jd()); }

  const std::u16string& key_list() const { return _readable_key_list; }
  const std::u16string& key_list_raw() const { return _key_list; }

  virtual void reset();
  void reset_key_list();
  BaseController& start(const std::vector<std::u16string>& ks);
  BaseController& hold(const std::vector<std::u16string>& ks);
  BaseController& end(const std::vector<std::u16string>& ks);
  BaseController& db_hit(const std::vector<std::u16string>& ks);
  BaseController& click(const std::vector<std::u16string>& ks);
  BaseController& dbl_click(const std::vector<std::u16string>& ks);
  BaseController& key_down(const std::vector<std::u16string>& ks);
  BaseController& key_up(const std::vector<std::u16string>& ks);

  BaseController& ku(const std::vector<std::u16string>& ks) { return key_up(ks); }
  BaseController& kd(const std::vector<std::u16string>& ks) { return key_down(ks); }
  BaseController& ck(const std::vector<std::u16string>& ks) { return click(ks); }

  bool is_hold(const std::u16string& k) const;
  bool is_hit(const std::u16string& k) const;
  bool is_end(const std::u16string& k) const;
  bool is_start(const std::u16string& k) const;
  bool is_db_hit(const std::u16string& k);

  bool tst(const std::u16string& type, const std::u16string& key);

  virtual const ControllerResult& update();

  bool sequence_keys_test(const std::u16string& str) const;
  bool sametime_keys_test(const std::u16string& str) const;

  ControllerKeyStatus keys;
  ControllerDoubleClicks dbc;
  ControllerResult result;
  std::vector<std::pair<Status, std::u16string>> queue;
  std::u16string player_id;
  Value player;

 private:
  bool check_key_act(const Value& map, const std::u16string& kind, KeyStatus* key, bool use);
  bool check_hit_seqs(const Value& seqs, ControllerResult& out);

  const KeyStatus* slot(const std::u16string& k) const { return keys.slot(k); }
  KeyStatus* slot(const std::u16string& k) { return keys.slot(k); }

  const CtrlEnv* _env = nullptr;
  const ICtrlCreator* _creator = nullptr;
  Times _time{10, 9007199254740991.0};
  std::u16string _key_list;
  std::u16string _readable_key_list;
  bool _is_human = false;
  bool _is_bot = false;
  bool _is_ball = false;
  SeqKeys _seq_djdj;
  SeqKeys _seq_dddd;
  SeqKeys _seq_dada;
  SeqKeys _seq_djjj;
};

}
}
