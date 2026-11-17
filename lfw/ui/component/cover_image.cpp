#include "lfw/ui/component/cover_image.h"

#include <algorithm>
#include <optional>

#include "lfw/ui/uinode.h"

namespace lfw {
namespace ui {

namespace {

/** `cover_scale`（`src/LFW/ui/cover_scale.ts`） */
double cover_scale(double parent_w, double parent_h, double design_w, double design_h) {
  if (parent_w <= 0.0 || parent_h <= 0.0 || design_w <= 0.0 || design_h <= 0.0) return 1.0;
  return std::max(parent_w / design_w, parent_h / design_h);
}

}  // namespace

const ClazzTag* CoverImage::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& CoverImage::TAGS() {
  static const std::vector<std::u16string> tags{u"CoverImage"};
  return tags;
}

void CoverImage::update(double dt) {
  (void)dt;
  const std::optional<double> design_w = props_holder.num(u"design_w");
  const std::optional<double> design_h = props_holder.num(u"design_h");
  if (!design_w.has_value() || !design_h.has_value()) return;

  const std::optional<std::u16string> target_name = props_holder.str(u"target");
  UINode* const target =
      (target_name.has_value() && *target_name == u"root") ? &node.root() : node.parent();
  if (target == nullptr) return;

  const double w = target->w();
  const double h = target->h();
  if (w <= 0.0 || h <= 0.0) return;

  const double scale = cover_scale(w, h, *design_w, *design_h);
  if (node.scale.x == scale && node.scale.y == scale) return;
  node.set_scale(scale, scale);
}

}  // namespace ui
}  // namespace lfw
