#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "lfw/ui/component/fade_out_opacity.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class LoadingContentText : public UIComponent {
 public:
  static constexpr const char* TAG = "LoadingContentText";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  double fade_out_duration();
  double fade_out_delay();

  void init() override;
  void on_resume() override;
  void on_pause() override;

  void on_loading_start();
  void on_loading_end();
  void on_progress(const std::u16string& text, double progress);
  void on_loading_failed(const Value& reason);
  std::u16string format_reason(const Value& reason);

 protected:
  FadeOutOpacity* fadeout = nullptr;
  std::unique_ptr<UIComponent> _fadeout_owner;
  std::optional<std::u16string> failed_reason;
  std::function<void()> _remover;
};

}
}
