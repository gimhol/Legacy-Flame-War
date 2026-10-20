#pragma once

#include <map>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/utils/eval_number.h"
#include "lfw/director/action_director.h"

namespace lfw {

class Entity;
class World;

namespace director {

class Director {
 public:
  Director(World& world, Value data);

  World& world;
  Value data;
  ActionDirector action_director;

  std::vector<Entity*> fighters;
  std::vector<Entity*> entities;
  std::vector<Entity*> weapons;

  const std::vector<Entity*>& spawned() const { return _spawned; }

  void start();
  void restart();
  void stop();
  void update(double dt);

  std::vector<Entity*> resolve(const Value& selectors) const;
  Entity* spawn(const std::u16string& oid);

 private:
  void update_auto();
  void compile();
  void setup_scene();
  void spawn_actor(const Value& actor);
  std::vector<std::u16string> actor_oids(const Value& actor) const;
  void actor_position(const Value& actor, double index, double count,
                      const utils::EvalContext& ctx, double& x, double& y, double& z) const;
  void apply_values(Entity& e, const Value& values, double index) const;
  void apply_ctrl(Entity& e, const Value& actor, double index, const std::u16string& oid);
  std::u16string player_id_of(const std::u16string& templ, const Entity& e, double index) const;
  utils::EvalContext eval_ctx() const;
  void run_action(const Value& action);
  void apply_keys(Entity& e, const Value& steps) const;
  void enter_frame(Entity& e, const std::u16string& id);
  void cycle(const Value& target, const std::u16string& property, const Value& values);
  void throw_spot(const Value& target, const std::u16string& from);
  bool match_auto(const Entity& e, const Value& when) const;
  double expr(const Value& value) const;

  std::map<std::u16string, std::vector<Entity*>> _actors;
  std::map<std::u16string, std::vector<Entity*>> _groups;
  std::vector<Entity*> _spawned;
  bool _compiled = false;
};

}
}
