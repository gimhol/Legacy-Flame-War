#pragma once

#include <string>
#include <vector>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

/** `TiledImage`（`src/LFW/ui/component/TiledImage.ts`）：按节点实际尺寸重算平铺次数。 */
class TiledImage : public UIComponent {
 public:
  static constexpr const char* TAG = "TiledImage";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void update(double dt) override;
};

}  // namespace ui
}  // namespace lfw
