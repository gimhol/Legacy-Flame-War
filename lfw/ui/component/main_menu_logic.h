#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ReachableGroup;

class MainMenuLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "MainMenuLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  MainMenuLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~MainMenuLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_resume() override;

  void apply();

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
