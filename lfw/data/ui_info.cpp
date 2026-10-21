#include "lfw/data/ui_info.h"

#include <cmath>
#include <utility>

#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace {

std::u16string str_or_empty(const Value& holder, const char16_t* key) {
  const Value v = field_or(holder, key);
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : std::u16string();
}

std::optional<double> opt_num(const Value* const v) {
  if (v == nullptr) return std::nullopt;
  const double* const d = std::get_if<double>(v);
  if (d == nullptr || std::isnan(*d)) return std::nullopt;
  return *d;
}

std::optional<bool> opt_bool(const Value* const v) {
  if (v == nullptr) return std::nullopt;
  const bool* const b = std::get_if<bool>(v);
  if (b == nullptr) return std::nullopt;
  return *b;
}

std::optional<std::u16string> opt_str(const Value* const v) {
  if (v == nullptr) return std::nullopt;
  const std::u16string* const s = std::get_if<std::u16string>(v);
  if (s == nullptr) return std::nullopt;
  return *s;
}

double num_or_nan(const Value& v) {
  const double* const d = std::get_if<double>(&v);
  return d != nullptr ? *d : std::nan("");
}

/** 与 `UINode::set3` 同语义：不是数组就保持默认值，缺的分量是 NaN。 */
Vector3 vec3_of(const Value* const v, Vector3 fallback) {
  const Array* const a = v != nullptr ? as_array(*v) : nullptr;
  if (a == nullptr) return fallback;
  fallback.set(a->size() > 0 ? num_or_nan(a->at(0)) : std::nan(""),
               a->size() > 1 ? num_or_nan(a->at(1)) : std::nan(""),
               a->size() > 2 ? num_or_nan(a->at(2)) : std::nan(""));
  return fallback;
}

std::u16string text_of(const Value* const v) {
  if (v == nullptr) return {};
  if (const std::u16string* const s = std::get_if<std::u16string>(v)) return *s;
  return to_string(*v);
}

UIAction action_of(const Value& v) {
  UIAction ret;
  const Object* const o = as_object(v);
  if (o == nullptr) {
    ret.name = text_of(&v);
    return ret;
  }
  ret.name = text_of(o->get(u"name"));
  const Value* const args = o->get(u"args");
  if (const Array* const list = args != nullptr ? as_array(*args) : nullptr) {
    for (const Value& a : list->items()) ret.args.push_back(text_of(&a));
  }
  return ret;
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

CookedUIInfo::CookedUIInfo(Value cooked) : _raw(std::move(cooked)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  _id = field_or(*o, u"id");
  _name = field_or(*o, u"name");
  _pos = vec3_of(o->get(u"pos"), _pos);
  _scale = vec3_of(o->get(u"scale"), _scale);
  _center = vec3_of(o->get(u"center"), _center);
  _size = vec3_of(o->get(u"size"), _size);
  _visible = opt_bool(o->get(u"visible"));
  _disabled = opt_bool(o->get(u"disabled"));
  _clips = opt_bool(o->get(u"clips"));
  _auto_focus = opt_bool(o->get(u"auto_focus"));
  _opacity = opt_num(o->get(u"opacity"));
  _count = opt_num(o->get(u"count"));
  _background_alpha = opt_num(o->get(u"backgroundAlpha"));
  _foreground_alpha = opt_num(o->get(u"foregroundAlpha"));
  _outline_width = opt_num(o->get(u"outlineWidth"));
  _outline_alpha = opt_num(o->get(u"outlineAlpha"));
  _color = opt_str(o->get(u"color"));
  _background = opt_str(o->get(u"background"));
  _foreground = opt_str(o->get(u"foreground"));
  _outline_color = opt_str(o->get(u"outlineColor"));
  _i18n = opt_str(o->get(u"i18n"));
  _img = UIImgInfo(field_or(*o, u"img"));
  _style = field_or(*o, u"style");
  _values = field_or(*o, u"values");
  _component = field_or(*o, u"component");
  _img_info = field_or(*o, u"img_info");
  _txt_info = field_or(*o, u"txt_info");
  _items = field_or(*o, u"items");
  _raw_info = field_or(*o, u"raw");

  const Value actions_v = field_or(*o, u"actions");
  if (const Object* const actions = as_object(actions_v)) {
    for (const std::u16string& place : actions->keys()) {
      const Value* const v = actions->get(place);
      if (v == nullptr) continue;
      std::vector<UIAction> list;
      if (const Array* const arr = as_array(*v)) {
        for (const Value& a : arr->items()) list.push_back(action_of(a));
      } else {
        list.push_back(action_of(*v));
      }
      _actions.emplace(place, std::move(list));
    }
  }
}

const std::vector<UIAction>* CookedUIInfo::actions_at(const std::u16string& place) const {
  const auto it = _actions.find(place);
  return it != _actions.end() ? &it->second : nullptr;
}

}