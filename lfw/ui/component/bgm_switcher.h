#pragma once

#include <string>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class BGMSwitcher : public UIComponent {
 public:
  static constexpr const char* TAG = "BGMSwitcher";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_resume() override;
  void on_pause() override;
  void on_click(LFWPointerEvent& e) override;

  std::u16string which();
  double offset();

 private:
  struct SoundsListener : public LfwSoundsCallback {
    explicit SoundsListener(BGMSwitcher* o) : owner(o) {}
    BGMSwitcher* owner = nullptr;
    void on_bgm_changed(const std::u16string& bgm, const std::u16string& prev) override;
  };

  std::u16string _which;
  SoundsListener _sounds_listener{this};
};

}
}
