#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ui/component/action_director.h"
#include "lfw/ui/component/component_fsm_state.h"

namespace lfw {

class Background;
class Entity;
class FSM;
class LFW;
class World;

namespace ui {

class Tests;

const std::vector<std::u16string>& agk();

class TestCase : public ComponentFSMState {
 public:
  static double key_counter;

  explicit TestCase(Tests& owner);
  ~TestCase() override;

  Value key() const override;
  std::optional<Value> update(double dt) override;
  void enter() override;
  void leave() override;

  virtual ActionDirector& director() { return _director; }

  UIComponent& owner_component() const { return owner; }

  std::vector<Entity*> entities;
  std::vector<Entity*> fighters;
  std::vector<Entity*> weapons;

  Background& bg() const;
  FSM& fsm() const;
  double left() const;
  double right() const;
  double far_plane() const;
  double near_plane() const;
  double midX() const;
  double midZ() const;

  Entity* spawn(const std::u16string& oid);
  std::vector<Entity*> spawns(const std::vector<std::u16string>& oids);
  std::vector<Entity*> bandits_8();
  std::vector<Entity*> bandits_mid_8(double x = 100.0, double z = 50.0);
  std::vector<Entity*> circle(const std::vector<std::u16string>& oids, double ox, double oz,
                              double r1, double r2, int count = -1);
  std::vector<Entity*> verti(const std::vector<std::u16string>& oids, double ox, double oz,
                             double h, int count = -1);
  std::vector<Entity*> hori(const std::vector<std::u16string>& oids, double ox, double oz, double w,
                            int count = -1);
  std::vector<Entity*> hori_3(const std::u16string& oid, double rx = 250.0,
                              std::optional<double> z = std::nullopt);
  std::vector<Entity*> hori_2(const std::u16string& oid, double rx = 250.0,
                              std::optional<double> z = std::nullopt);
  std::vector<Entity*> verti_3(const std::u16string& oid, std::optional<double> x = std::nullopt,
                               double z = 200.0);

 private:
  double _key = 0.0;
  ActionDirector _director;
};

}
}
