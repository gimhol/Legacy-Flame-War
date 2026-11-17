#pragma once

#include <string>
#include <vector>

#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

/** `CoverImage`（`src/LFW/ui/component/CoverImage.ts`）：按目标区域等比覆盖（`cover_scale`）。 */
class CoverImage : public UIComponent {
 public:
  static constexpr const char* TAG = "CoverImage";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }

  void update(double dt) override;
};

}  // namespace ui
}  // namespace lfw
