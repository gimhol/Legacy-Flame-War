#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/animation/sequence.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class OpacityFlash : public UIComponent {
 public:
  static constexpr const char* TAG = "OpacityFlash";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using UIComponent::UIComponent;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  void on_start() override;
  void update(double dt) override;

  void start() override;
  void stop() override;
  void replay() override;

  bool is_playable() const override { return true; }

  Sequence& anim() { return _anim; }

 protected:
  Sequence _anim;
  std::vector<std::unique_ptr<Animation>> _owned;
};

}
}
