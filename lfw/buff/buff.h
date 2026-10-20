#pragma once

#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/entity/entity.h"
#include "lfw/utils/times.h"

namespace lfw {

class LFW;
class World;

namespace buff {

class Buff;


class Buff {
 public:
  virtual ~Buff() = default;
  Buff(LFW& lfw, const std::u16string& id, const Value& kind);

  LFW& lfw() const { return *_lfw; }
  World& world() const { return *_world; }

  const std::u16string& id() const { return _id; }
  const std::vector<std::u16string>& victims() const { return _victims; }
  bool dead() const { return _lifetime.remains() == 0; }
  double lifetime() const { return _lifetime.value(); }
  void set_lifetime(double v) { _lifetime.set_value(v); }
  double duration() const { return _lifetime.max(); }
  void set_duration(double v) { _lifetime.set_max(v); }
  double ticks() const { return _ticker.max(); }
  void set_ticks(double v) { _ticker.set_max(v); }
  const Value& kind() const { return _kind; }
  double level() const { return _level; }
  void set_level(double v) { _level = v; }
  bool mounted() const { return _mounted; }
  const std::u16string& attacker_id() const { return _attacker_id; }
  size_t effects_size() const { return _effects.size(); }
  Entity* attacter();

  void reset(const std::u16string& id);
  void rebind_lfw(LFW& lfw);
  virtual void init() {}
  void set_attacker_by_id(const std::u16string& id);
  void set_attacker_entity(Entity* e);
  void set_victim(Entity* victim);
  void add_victim(Entity* victim);
  void del_victim(Entity* victim);
  void update(double d);
  Value to_snapshot() const;
  void read_snapshot(const Value& s);
  virtual void mount();
  virtual void unmount();

 protected:
  virtual std::u16string effect_oid() const { return std::u16string(); }
  virtual std::u16string effect_frame_id() const { return std::u16string(u"0"); }
  virtual bool has_on_update() const { return false; }
  virtual bool has_on_tick() const { return false; }
  virtual bool has_on_end() const { return false; }
  virtual void on_update(Entity* attacker, Entity* victim) {
    (void)attacker;
    (void)victim;
  }
  virtual void on_tick(Entity* attacker, Entity* victim) {
    (void)attacker;
    (void)victim;
  }
  virtual void on_end(Entity* attacker, Entity* victim) {
    (void)attacker;
    (void)victim;
  }
  Value effect_data();
  virtual void place_effect(Entity* effect, Entity* victim);
  void place_effect_center(Entity* effect, Entity* victim);
  void del_effect(const std::u16string& vid);
  void clear_effects();
  void show_effect(Entity* victim);
  void update_effects();
  bool del_id(const std::u16string& id);
  void drop_cached_refs();

  LFW* _lfw = nullptr;
  World* _world = nullptr;
  std::u16string _id;
  Value _kind;
  Entity* _attacker = nullptr;
  std::u16string _attacker_id;
  double _level = 0;
  bool _mounted = false;
  std::vector<std::u16string> _victims;
  Times _ticker;
  Times _lifetime{0.0, 1.0};
  std::vector<std::pair<std::u16string, Entity*>> _effects;
  Value _effect_data;
  bool _effect_data_loaded = false;
};

Buff* grant_buff(const std::u16string& kind, Entity* attacker, Entity* victim,
                 double duration = 0);

}
}
