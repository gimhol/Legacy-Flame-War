#pragma once

#include <string>
#include <vector>

#include "lfw/base/no_emit_callbacks.h"
#include "lfw/core/value.h"
#include "lfw/ui/component/action_director.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

class Entity;

namespace ui {

class LittleFunnyAutoGame : public UIComponent {
 public:
  static constexpr const char* TAG = "LittleFunnyAutoGame";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  LittleFunnyAutoGame(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }

  void update(double dt) override;
  void on_resume() override;
  void on_pause() override;

  Entity* add_random_fighter(int lr, const std::u16string& team,
                             const Value& data = Value());

  Entity* boss(int team) const;
  const std::vector<Entity*>& soldiers(int team) const;
  int lr() const { return _lr; }
  int ox() const { return _ox; }

 private:
  using ValueListener = NoEmitCallbacksT<Value>::Listener;
  using ValuePayloads = NoEmitCallbacksT<Value>::Payloads;

  struct Teams {
    Entity* boss = nullptr;
    std::vector<Entity*> soldiers;
  };

  void on_fighter_dead(const Value& ref);
  void handle_soldiers();

  Value _datas = Value(std::make_shared<Array>());
  int _lr = 1;
  int _ox = 1;
  Teams _team1;
  Teams _team2;

  struct FighterListenerT : ValueListener {
    explicit FighterListenerT(LittleFunnyAutoGame* o);
  };
  FighterListenerT _fighter_cbs{this};

  ActionDirector _director;
};

}
}
