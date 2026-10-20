#pragma once

#include <functional>
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

  using UIComponent::UIComponent;

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
  SliderHandle* row_slider(const std::u16string& row_id);
  ArrowSwitcher* row_switcher(const std::u16string& row_id);

  struct SoundsListener : LfwSoundsCallback {
    MiscSettingsLogic* self = nullptr;
    void on_volume_changed(double volume, double prev) override;
    void on_bgm_volume_changed(double volume, double prev) override;
    void on_sound_volume_changed(double volume, double prev) override;
    void on_bgm_muted_changed(bool muted) override;
    void on_sound_muted_changed(bool muted) override;
  } _sounds_listener;
  bool _sounds_added = false;

  std::function<void()> _world_remover;
  std::function<void()> _lfw_remover;

  SliderHandle* _a = nullptr;
  SliderHandle* _b = nullptr;
  SliderHandle* _c = nullptr;
  SliderHandle* _d = nullptr;
  SliderHandle* _e = nullptr;
  SliderHandle* _f = nullptr;
  SliderHandle* _g = nullptr;
  SliderHandle* _h = nullptr;
  ArrowSwitcher* _i = nullptr;
  SliderHandle* _j = nullptr;
};

}
}
