#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/ui/component/slider_handle.h"

namespace lfw {
namespace ui {

class SwitcherHandle : public SliderHandle {
 public:
  static constexpr const char* TAG = "SwitcherHandle";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using SliderHandle::SliderHandle;

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  void on_start() override;
  void on_resume() override;
  void update(double dt) override;

  std::u16string mode() override;

  UINode* options();
  double slots();

 protected:
  void sync_range();
  double cover_index();
  void arrange_options(std::optional<double> cover = {});
};

}
}
