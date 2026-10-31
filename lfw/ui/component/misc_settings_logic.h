#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class ArrowSwitcher;
class SliderHandle;

class MiscSettingsLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "MiscSettingsLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  MiscSettingsLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~MiscSettingsLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  SliderHandle* bgm_toggle();
  SliderHandle* bgm_volume();
  SliderHandle* sfx_toggle();
  SliderHandle* sfx_volume();
  SliderHandle* team_outline();
  SliderHandle* render_rate();
  SliderHandle* main_volume();
  SliderHandle* ups();
  ArrowSwitcher* lang_row();
  SliderHandle* stats_visible();

  void on_start() override;
  void on_resume() override;
  void on_stop() override;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
