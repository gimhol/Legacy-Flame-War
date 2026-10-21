#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

/**
 * `IUIImgInfo`（`src/LFW/ui/IUIImgInfo.dat.ts`）。
 *
 * 每个字段在 TS 里都是 `number`，但数据链路上允许写成表达式串，所以这里存 `Value`；
 * 好处是字段名、默认值、取值点只有这一份，不再是满地 `field_or(img, u"x")`。
 * 裁剪/缩放语义见 TS 注释：先按 (x,y,w,h) 裁原图，再缩放到 (dw,dh)，最后按 flip 翻转。
 */
class UIImgInfo {
 public:
  UIImgInfo() = default;
  explicit UIImgInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& path() const { return _path; }
  const Value& x() const { return _x; }
  const Value& y() const { return _y; }
  const Value& w() const { return _w; }
  const Value& h() const { return _h; }
  const Value& dw() const { return _dw; }
  const Value& dh() const { return _dh; }
  const Value& flip_x() const { return _flip_x; }
  const Value& flip_y() const { return _flip_y; }
  const Value& wrapS() const { return _wrap_s; }
  const Value& wrapT() const { return _wrap_t; }
  const Value& offsetX() const { return _offset_x; }
  const Value& offsetY() const { return _offset_y; }
  const Value& offsetAnimX() const { return _offset_anim_x; }
  const Value& offsetAnimY() const { return _offset_anim_y; }
  const Value& offsetAnimR() const { return _offset_anim_r; }
  const Value& repeatX() const { return _repeat_x; }
  const Value& repeatY() const { return _repeat_y; }
  const Value& nine_patch() const { return _nine_patch; }

  /** 表达式引擎 / JS 边界专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  std::u16string _path;
  Value _x;
  Value _y;
  Value _w;
  Value _h;
  Value _dw;
  Value _dh;
  Value _flip_x;
  Value _flip_y;
  Value _wrap_s;
  Value _wrap_t;
  Value _offset_x;
  Value _offset_y;
  Value _offset_anim_x;
  Value _offset_anim_y;
  Value _offset_anim_r;
  Value _repeat_x;
  Value _repeat_y;
  Value _nine_patch;
};

/**
 * `IUIInfo`（`src/LFW/ui/IUIInfo.dat.ts`）。
 *
 * 与 `UIImgInfo` 同理：TS 里多数字段是 `T | string`（`ToStringable`），
 * 所以字段类型保留 `Value`，这里负责的是"字段清单 + 一次性取值"。
 */
class UIInfo {
 public:
  UIInfo() = default;
  explicit UIInfo(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  /** 原始值（可能是数字），UI 侧要原样往外给。 */
  const Value& id_value() const { return _id; }
  const Value& name_value() const { return _name; }
  const Value& pos() const { return _pos; }
  const Value& opacity() const { return _opacity; }
  const Value& center() const { return _center; }
  const Value& scale() const { return _scale; }
  const Value& size() const { return _size; }
  const Value& visible() const { return _visible; }
  const Value& disabled() const { return _disabled; }
  const Value& clips() const { return _clips; }
  const Value& color() const { return _color; }
  const Value& background() const { return _background; }
  const Value& backgroundAlpha() const { return _background_alpha; }
  const Value& foreground() const { return _foreground; }
  const Value& foregroundAlpha() const { return _foreground_alpha; }
  const Value& outlineColor() const { return _outline_color; }
  const Value& outlineWidth() const { return _outline_width; }
  const Value& outlineAlpha() const { return _outline_alpha; }
  const UIImgInfo& img() const { return _img; }
  const Value& i18n() const { return _i18n; }
  const Value& style() const { return _style; }
  const Value& dev_component() const { return _dev_component; }
  const Value& component() const { return _component; }
  const Value& actions() const { return _actions; }
  const Value& items() const { return _items; }
  const Value& auto_focus() const { return _auto_focus; }
  const Value& template_() const { return _template; }
  const Value& count() const { return _count; }
  const Value& template_values() const { return _template_values; }
  const Value& values() const { return _values; }
  const Value& templates() const { return _templates; }

  /** 表达式引擎 / JS 边界专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  Value _id;
  Value _name;
  Value _pos;
  Value _opacity;
  Value _center;
  Value _scale;
  Value _size;
  Value _visible;
  Value _disabled;
  Value _clips;
  Value _color;
  Value _background;
  Value _background_alpha;
  Value _foreground;
  Value _foreground_alpha;
  Value _outline_color;
  Value _outline_width;
  Value _outline_alpha;
  UIImgInfo _img;
  Value _i18n;
  Value _style;
  Value _dev_component;
  Value _component;
  Value _actions;
  Value _items;
  Value _auto_focus;
  Value _template;
  Value _count;
  Value _template_values;
  Value _values;
  Value _templates;
};

}
