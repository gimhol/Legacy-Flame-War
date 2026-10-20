#pragma once

#include <optional>
#include <string>

#include "lfw/ui/component/ui_component.h"
#include "lfw/ui/style.h"

namespace lfw {
namespace ui {

class PlayerScore;

class PlayerScoreCell : public UIComponent {
 public:
  static constexpr const char* TAG = "PlayerScoreCell";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::u16string kind();
  PlayerScore* player_score();

  void update(double dt) override;

 protected:
  std::optional<std::u16string> _last_txt;
  double _last_style_version = -1;

  Style& get_style();
  std::u16string get_txt();
};

}
}
