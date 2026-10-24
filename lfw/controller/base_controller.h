#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "lfw/controller/controller_double_clicks.h"
#include "lfw/controller/controller_key_status.h"
#include "lfw/controller/controller_result.h"
#include "lfw/controller/key_status.h"
#include "lfw/controller/seq_keys.h"
#include "lfw/core/value.h"
#include "lfw/data/hit_key_map.h"
#include "lfw/defines/bin_op.h"
#include "lfw/utils/times.h"

namespace lfw {

class Entity;
class LFW;
class MersenneTwister;
class World;
class ICtrlCreator;

namespace controller {

enum class Status { UP = 0, DOWN = 1, HOLD = 2 };

class BaseController {
 public:
  static constexpr const char* TAG = "BaseController";

  BaseController();

  Entity* entity() const { return _entity; }
  virtual void set_entity(Entity* e) { _entity = e; }
  World* world() const;
  LFW* lfw() const;
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
  BaseController& start(std::initializer_list<const char16_t*> ks);
  BaseController& hold(std::initializer_list<const char16_t*> ks);
  BaseController& end(std::initializer_list<const char16_t*> ks);
  BaseController& db_hit(std::initializer_list<const char16_t*> ks);
  BaseController& click(std::initializer_list<const char16_t*> ks);
  BaseController& dbl_click(std::initializer_list<const char16_t*> ks);
  BaseController& key_down(std::initializer_list<const char16_t*> ks);
  BaseController& key_up(std::initializer_list<const char16_t*> ks);

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
  double key_hit_duration() const;
  double double_click_interval() const;
  bool check_key_act(const HitKeyMap& map, const std::u16string& kind, KeyStatus* key, bool use);
  bool check_hit_seqs(const Value& seqs, ControllerResult& out);

  const KeyStatus* slot(const std::u16string& k) const { return keys.slot(k); }
  KeyStatus* slot(const std::u16string& k) { return keys.slot(k); }

  Entity* _entity = nullptr;
  const ICtrlCreator* _creator = nullptr;
  Times _time{10, Times::MAX};
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
