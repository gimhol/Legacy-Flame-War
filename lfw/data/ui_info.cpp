#include "lfw/data/ui_info.h"

#include <utility>

#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/type_cast.h"

namespace lfw {
namespace {

std::u16string str_or_empty(const Value& holder, const char16_t* key) {
  const Value v = field_or(holder, key);
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : std::u16string();
}

}

UIImgInfo::UIImgInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _path = str_or_empty(_raw, u"path");
  _x = field_or(*o, u"x");
  _y = field_or(*o, u"y");
  _w = field_or(*o, u"w");
  _h = field_or(*o, u"h");
  _dw = field_or(*o, u"dw");
  _dh = field_or(*o, u"dh");
  _flip_x = field_or(*o, u"flip_x");
  _flip_y = field_or(*o, u"flip_y");
  _wrap_s = field_or(*o, u"wrapS");
  _wrap_t = field_or(*o, u"wrapT");
  _offset_x = field_or(*o, u"offsetX");
  _offset_y = field_or(*o, u"offsetY");
  _offset_anim_x = field_or(*o, u"offsetAnimX");
  _offset_anim_y = field_or(*o, u"offsetAnimY");
  _offset_anim_r = field_or(*o, u"offsetAnimR");
  _repeat_x = field_or(*o, u"repeatX");
  _repeat_y = field_or(*o, u"repeatY");
  _nine_patch = field_or(*o, u"nine_patch");
}

UIInfo::UIInfo(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = field_or(*o, u"id");
  _name = field_or(*o, u"name");
  _pos = field_or(*o, u"pos");
  _opacity = field_or(*o, u"opacity");
  _center = field_or(*o, u"center");
  _scale = field_or(*o, u"scale");
  _size = field_or(*o, u"size");
  _visible = field_or(*o, u"visible");
  _disabled = field_or(*o, u"disabled");
  _clips = field_or(*o, u"clips");
  _color = field_or(*o, u"color");
  _background = field_or(*o, u"background");
  _background_alpha = field_or(*o, u"backgroundAlpha");
  _foreground = field_or(*o, u"foreground");
  _foreground_alpha = field_or(*o, u"foregroundAlpha");
  _outline_color = field_or(*o, u"outlineColor");
  _outline_width = field_or(*o, u"outlineWidth");
  _outline_alpha = field_or(*o, u"outlineAlpha");
  _img = UIImgInfo(field_or(*o, u"img"));
  _i18n = field_or(*o, u"i18n");
  _style = field_or(*o, u"style");
  _dev_component = field_or(*o, u"dev_component");
  _component = field_or(*o, u"component");
  _actions = field_or(*o, u"actions");
  _items = field_or(*o, u"items");
  _auto_focus = field_or(*o, u"auto_focus");
  _template = field_or(*o, u"template");
  _count = field_or(*o, u"count");
  _template_values = field_or(*o, u"template_values");
  _values = field_or(*o, u"values");
  _templates = field_or(*o, u"templates");
}

}
