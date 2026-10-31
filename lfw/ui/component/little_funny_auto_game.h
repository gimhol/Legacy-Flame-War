#pragma once

#include <memory>
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
  ~LittleFunnyAutoGame() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  void update(double dt) override;
  void on_resume() override;
  void on_pause() override;

  Entity* add_random_fighter(int lr, const std::u16string& team,
                             const Value& data = Value());

  Entity* boss(int team) const;
  const std::vector<Entity*>& soldiers(int team) const;
  int lr() const;
  int ox() const;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
