#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/core/value.h"
#include "lfw/data/stage_phase_info.h"
#include "lfw/helper/randoming.h"
#include "lfw/utils/times.h"

namespace lfw {

class MersenneTwister;
class Entity;

namespace stage {

class Stage;


class Item {
 public:
  using RandomingOfItems = RandomingT<std::shared_ptr<Randoming>>;

  Item(Stage* stage, const StagePhaseInfo& phase, const StageObjectInfo& info);
  ~Item();

  Stage* stage() const { return _stage; }
  const StagePhaseInfo& phase() const { return *_phase; }
  const StageObjectInfo& info() const { return *_info; }
  const std::vector<Entity*>& objects() const { return _objects; }
  const Times& end_delay() const { return _end_delay; }
  bool released() const { return _released; }
  bool is_fighter() const { return _is_fighter; }

  std::optional<double> times;
  Value data;
  std::shared_ptr<RandomingOfItems> randoming;

  void update();
  bool spawn();
  void release();

 private:
  struct Watch {
    Entity* e = nullptr;
    Callbacks::Remover off_team;
    Callbacks::Remover off_dead;
  };
  void forget(Entity* e);

  Stage* _stage = nullptr;
  const StagePhaseInfo* _phase = nullptr;
  const StageObjectInfo* _info = nullptr;
  std::vector<Entity*> _objects;
  std::vector<Watch> _watches;
  Times _end_delay;
  bool _released = false;
  bool _is_fighter = false;
};

}
}
