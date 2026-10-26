#include "host/godot/lfw_godot_text_draw.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <godot_cpp/variant/string.hpp>

namespace lfw::host {

void draw_text_run(godot::CanvasItem& canvas, const godot::Ref<godot::Font>& font,
                   const godot::Vector2& pos, const std::string& line, int font_size,
                   const godot::Color& color, bool has_stroke, double stroke_w,
                   const godot::Color& stroke_color) {
  const godot::String text = godot::String::utf8(line.c_str());
  if (has_stroke && stroke_w > 0.0) {
    // Godot 的 outline 是「居中」描边：`outline_size` 有一半压在字里（被 fill 盖住），
    // 所以「往外 stroke_w 个逻辑像素」要按 2 倍换算；下界 2 = 至少露 1 像素（和旧那套 8 向平移一致）。
    const int outline = std::max(2, static_cast<int>(std::lround(stroke_w * 2.0)));
    canvas.draw_string_outline(font, pos, text, godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size,
                               outline, stroke_color);
  }
  canvas.draw_string(font, pos, text, godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size, color);
}

void draw_world_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item,
                          const godot::Ref<godot::Font>& font, int font_size) {
  const godot::Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                           static_cast<float>(item.b), static_cast<float>(item.a));
  const bool has_stroke = item.stroke_w > 0.0 && item.stroke_a > 0.0;
  const godot::Color stroke_color(
      static_cast<float>(item.stroke_r), static_cast<float>(item.stroke_g),
      static_cast<float>(item.stroke_b), static_cast<float>(item.stroke_a));
  const double text_w = static_cast<double>(
      font->get_string_size(godot::String::utf8(item.text.c_str()),
                            godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size)
          .x);
  const double ascent = static_cast<double>(font->get_ascent(font_size));
  const double descent = static_cast<double>(font->get_descent(font_size));
  const double block_h = ascent + descent;
  double cx = item.x;
  if (item.has_clamp_x) {
    const double hw = text_w * 0.5 + item.clamp_pad;
    const double lo = item.clamp_x0 + hw;
    const double hi = item.clamp_x1 - hw;
    cx = hi >= lo ? std::min(std::max(cx, lo), hi) : (lo + hi) * 0.5;
  }
  const double center_y = item.text_below ? item.y + block_h : item.y - block_h * 0.5;
  double cy = center_y;
  if (item.has_clamp_y) {
    const double hh = block_h * 0.5 + item.clamp_pad_y;
    const double lo = item.clamp_y0 + hh;
    const double hi = item.clamp_y1 - hh;
    cy = hi >= lo ? std::min(std::max(cy, lo), hi) : (lo + hi) * 0.5;
  }
  const double left = cx - text_w * 0.5;
  const double baseline = cy - block_h * 0.5 + ascent;
  draw_text_run(canvas, font, godot::Vector2(static_cast<float>(left), static_cast<float>(baseline)),
                item.text, font_size, color, has_stroke, item.stroke_w, stroke_color);
}

void draw_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item,
                    const TextFontFn& font_for_css) {
  if (item.text.empty()) return;
  godot::Ref<godot::Font> font = font_for_css(item.font_css);
  if (font.is_null()) return;
  const int size = static_cast<int>(std::lround(item.font_size > 0.0 ? item.font_size : 9.0));
  const int font_size = size > 0 ? size : 1;
  if (item.world_text) {
    draw_world_text_item(canvas, item, font, font_size);
    return;
  }
  const godot::Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                           static_cast<float>(item.b), static_cast<float>(item.a));
  const bool has_stroke = item.stroke_w > 0.0 && item.stroke_a > 0.0;
  const godot::Color stroke_color(
      static_cast<float>(item.stroke_r), static_cast<float>(item.stroke_g),
      static_cast<float>(item.stroke_b), static_cast<float>(item.stroke_a));
  const double pad = 2.0;
  std::vector<std::pair<std::string, double>> lines;
  double line_max = 0.0;
  for (std::size_t start = 0;;) {
    const std::size_t nl = item.text.find('\n', start);
    const std::string line =
        item.text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    double lw = 0.0;
    if (!line.empty()) {
      lw = static_cast<double>(
          font->get_string_size(godot::String::utf8(line.c_str()),
                                godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f, font_size)
              .x);
      line_max = std::max(line_max, lw);
    }
    lines.emplace_back(line, lw);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  const double block_w = line_max + pad * 2.0;
  const float line_h = font->get_height(font_size);
  const double block_h =
      static_cast<double>(lines.size()) * static_cast<double>(line_h) + pad * 2.0;
  const double content_x = item.x + item.w * 0.5 - block_w * 0.5 + pad;
  const double block_top = item.y + item.h * 0.5 - block_h * 0.5;
  double y = block_top + pad + static_cast<double>(font->get_ascent(font_size));
  for (const std::pair<std::string, double>& line : lines) {
    if (!line.first.empty()) {
      double ax = content_x;
      if (item.text_align == 1) {
        ax = content_x + std::round(line_max * 0.5) - line.second * 0.5;
      } else if (item.text_align == 2) {
        ax = content_x + std::round(line_max) - line.second;
      }
      draw_text_run(canvas, font,
                    godot::Vector2(static_cast<float>(ax), static_cast<float>(y)), line.first,
                    font_size, color, has_stroke, item.stroke_w, stroke_color);
    }
    y += static_cast<double>(line_h);
  }
}

}
