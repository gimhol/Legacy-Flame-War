#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/defines/i_vector3.h"

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

/** `IUIAction`（`src/LFW/ui/IUIAction.ts`）。cook 之后动作一定是 `{name, args}`。 */
struct UIAction {
  std::u16string name;
  std::vector<std::u16string> args;
};

/**
 * `ICookedUIInfo`（`src/LFW/ui/ICookedUIInfo.ts`）。
 *
 * 与 raw 的 `IUIInfo` 不同，cook 阶段已经把表达式全部求值过，所以这里的字段是
 * 具体类型：几何是 `Vector3`、开关是 `bool`、数值/字符串是 `std::optional`
 * （缺失或 `null` → `nullopt`）。
 *
 * 仍保留 `Value` 的只有 `style` / `values` / `component` / `img_info` / `txt_info` /
 * `items`：它们的形态由 `IStyle`、组件系统或 host 渲染后端决定，不属于这份类型。
 */
class CookedUIInfo {
 public:
  CookedUIInfo() = default;
  explicit CookedUIInfo(Value cooked);

  bool valid() const { return as_object(_raw) != nullptr; }

  /** 原始值（cooked 里 id/name 可能是数字），UI 侧要原样往外给。 */
  const Value& id_value() const { return _id; }
  const Value& name_value() const { return _name; }

  const Vector3& pos() const { return _pos; }
  const Vector3& scale() const { return _scale; }
  const Vector3& center() const { return _center; }
  const Vector3& size() const { return _size; }

  std::optional<bool> visible() const { return _visible; }
  std::optional<bool> disabled() const { return _disabled; }
  std::optional<bool> clips() const { return _clips; }
  std::optional<bool> auto_focus() const { return _auto_focus; }

  std::optional<double> opacity() const { return _opacity; }
  std::optional<double> count() const { return _count; }
  std::optional<double> backgroundAlpha() const { return _background_alpha; }
  std::optional<double> foregroundAlpha() const { return _foreground_alpha; }
  std::optional<double> outlineWidth() const { return _outline_width; }
  std::optional<double> outlineAlpha() const { return _outline_alpha; }

  const std::optional<std::u16string>& color() const { return _color; }
  const std::optional<std::u16string>& background() const { return _background; }
  const std::optional<std::u16string>& foreground() const { return _foreground; }
  const std::optional<std::u16string>& outlineColor() const { return _outline_color; }
  const std::optional<std::u16string>& i18n() const { return _i18n; }

  /** `{[place]: IUIAction[]}`（`TUIActionPlace`），该 place 不存在时返回 `nullptr`。 */
  const std::vector<UIAction>* actions_at(const std::u16string& place) const;

  const UIImgInfo& img() const { return _img; }
  const Value& style() const { return _style; }
  const Value& values() const { return _values; }
  const Value& component() const { return _component; }
  const Value& img_info() const { return _img_info; }
  const Value& txt_info() const { return _txt_info; }
  const Value& items() const { return _items; }
  /** 未经 cook 的 `IUIInfo`。 */
  const Value& raw() const { return _raw_info; }

  /** 表达式引擎 / JS 边界专用。 */
  const Value& to_value() const { return _raw; }

 private:
  Value _raw;
  Value _id;
  Value _name;
  Vector3 _pos;
  Vector3 _scale{1, 1, 1};
  Vector3 _center;
  Vector3 _size;
  std::optional<bool> _visible;
  std::optional<bool> _disabled;
  std::optional<bool> _clips;
  std::optional<bool> _auto_focus;
  std::optional<double> _opacity;
  std::optional<double> _count;
  std::optional<double> _background_alpha;
  std::optional<double> _foreground_alpha;
  std::optional<double> _outline_width;
  std::optional<double> _outline_alpha;
  std::optional<std::u16string> _color;
  std::optional<std::u16string> _background;
  std::optional<std::u16string> _foreground;
  std::optional<std::u16string> _outline_color;
  std::optional<std::u16string> _i18n;
  std::map<std::u16string, std::vector<UIAction>> _actions;
  UIImgInfo _img;
  Value _style;
  Value _values;
  Value _component;
  Value _img_info;
  Value _txt_info;
  Value _items;
  Value _raw_info;
};

}