#pragma once

#include <functional>
#include <string>

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "host/godot/lfw_godot_renderer.h"

namespace lfw::host {

/** `font_css` → 字体（宿主那边的 `FontCache`） */
using TextFontFn = std::function<godot::Ref<godot::Font>(const std::string& css)>;

/** 一行字，可选描边（Godot 的 outline 是居中描边 ⇒ 宽度按 2 倍换算） */
void draw_text_run(godot::CanvasItem& canvas, const godot::Ref<godot::Font>& font,
                   const godot::Vector2& pos, const std::string& line, int font_size,
                   const godot::Color& color, bool has_stroke, double stroke_w,
                   const godot::Color& stroke_color);

/** 世界里的浮动文字：以 `item.x/y` 为锚点，按 `has_clamp_*` 夹进给定包围盒 */
void draw_world_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item,
                          const godot::Ref<godot::Font>& font, int font_size);

/** 通用文字 item（世界浮动文字 / UI 多行文字），字体由 `font_for_css` 取 */
void draw_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item,
                    const TextFontFn& font_for_css);

}
