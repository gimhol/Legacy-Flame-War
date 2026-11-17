#include "lfw/ui/component/tiled_image.h"

#include <memory>
#include <optional>

#include "lfw/ui/uinode.h"

namespace lfw {
namespace ui {

namespace {

double num_or(const Object& o, const char16_t* key, double dflt) {
  const Value* const v = o.get(key);
  if (v == nullptr) return dflt;
  const double n = to_number(*v);
  return std::isnan(n) ? dflt : n;
}

}  // namespace

const ClazzTag* TiledImage::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& TiledImage::TAGS() {
  static const std::vector<std::u16string> tags{u"TiledImage", u"TileImage"};
  return tags;
}

void TiledImage::update(double dt) {
  (void)dt;
  const std::optional<double> design_w = props_holder.num(u"design_w");
  const std::optional<double> design_h = props_holder.num(u"design_h");
  const std::optional<double> repeat_x = props_holder.num(u"repeat_x");
  const std::optional<double> repeat_y = props_holder.num(u"repeat_y");
  if (!design_w.has_value() || !design_h.has_value() || !repeat_x.has_value() ||
      !repeat_y.has_value()) {
    return;
  }
  if (*design_w == 0.0 || *design_h == 0.0) return;
  const double w = node.w();
  const double h = node.h();
  if (w <= 0.0 || h <= 0.0) return;

  const double rx = w / *design_w * *repeat_x;
  const double ry = h / *design_h * *repeat_y;
  const Value image = node.image();
  const Object* const src = as_object(image);
  if (src == nullptr) return;
  if (num_or(*src, u"repeatX", 1.0) == rx && num_or(*src, u"repeatY", 1.0) == ry) return;

  auto copy = std::make_shared<Object>();
  for (const std::u16string& k : src->keys()) copy->set(k, *src->get(k));
  copy->set(u"repeatX", Value(rx));
  copy->set(u"repeatY", Value(ry));
  node.set_image(Value(copy));
}

}  // namespace ui
}  // namespace lfw
