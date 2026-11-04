#include "host/godot/lfw_godot_text_input.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <utility>

#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>

#include "lfw/ui/component/text_input.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/utf8.h"

namespace lfw::host {

namespace {

/** 独立画布层：游戏 UI 画在宿主 canvas 的 z>=1 上，控制台用 100，这里占 99 */
constexpr int kOverlayLayer = 99;

std::u16string from_utf8(const std::string& s) {
  return decode_utf8(std::vector<uint8_t>(s.begin(), s.end()));
}

std::string to_utf8(const std::u16string& s) {
  const std::vector<uint8_t> buf = encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

}

godot::CanvasLayer* GodotTextInputs::ensure_layer(godot::Node& owner) {
  _owner = &owner;
  if (_layer != nullptr) return _layer;
  _layer = memnew(godot::CanvasLayer);
  _layer->set_name("lfw_text_input_layer");
  _layer->set_layer(kOverlayLayer);
  owner.add_child(_layer);
  return _layer;
}

void GodotTextInputs::sync(godot::Node& owner, const std::vector<GodotTextInputBox>& boxes,
                           const godot::Transform2D& to_canvas, const FontFn& font_for_css) {
  _owner = &owner;
  if (boxes.empty()) {
    destroy();
    return;
  }
  godot::CanvasLayer* const layer = ensure_layer(owner);
  const double scale_y = std::fabs(static_cast<double>(to_canvas.get_scale().y));
  std::vector<Overlay> next;
  next.reserve(boxes.size());
  for (const GodotTextInputBox& box : boxes) {
    Overlay* overlay = nullptr;
    // 同一个 `ui::UINode` 的编辑框优先复用：重建会丢焦点、丢选区，打字打到一半就断
    for (Overlay& old : _overlays) {
      if (old.edit == nullptr || old.node != box.node) continue;
      next.push_back(std::move(old));
      old.edit = nullptr;
      old.node = nullptr;
      overlay = &next.back();
      break;
    }
    if (overlay == nullptr) {
      Overlay created;
      created.edit = memnew(godot::LineEdit);
      char name_buf[48];
      std::snprintf(name_buf, sizeof(name_buf), "lfw_text_input_%d",
                    static_cast<int>(next.size() + 1));
      created.edit->set_name(name_buf);
      created.edit->set_mouse_filter(godot::Control::MOUSE_FILTER_STOP);
      // 边框 / 底色由 UI 自己画，`LineEdit` 只留光标和字
      godot::Ref<godot::StyleBoxEmpty> flat;
      flat.instantiate();
      for (const char* const state : {"normal", "focus", "read_only"}) {
        created.edit->add_theme_stylebox_override(state, flat);
      }
      layer->add_child(created.edit);
      created.node = box.node;
      created.input = box.input;
      created.shown = box.input->text();
      created.edit->set_text(godot::String::utf8(to_utf8(created.shown).c_str()));
      next.push_back(std::move(created));
      overlay = &next.back();
    }

    overlay->node = box.node;
    overlay->input = box.input;
    const std::optional<double> max_len = box.input->max_length();
    const double font_size = box.font_size_override > 0.0 ? box.font_size_override : box.font_size;
    const std::string& font_css =
        box.font_family_css.empty() ? box.font_css : box.font_family_css;
    const int font_px = std::max(
        1, static_cast<int>(std::lround(font_size * (scale_y > 0.0 ? scale_y : 1.0))));
    char sig[320];
    std::snprintf(sig, sizeof(sig), "%s|%.3f|%.3f,%.3f,%.3f,%.3f|%d|%d|%d", font_css.c_str(),
                  font_size, box.r, box.g, box.b, box.a, box.text_align, font_px,
                  max_len.has_value() ? static_cast<int>(*max_len) : -1);
    if (std::string(sig) != overlay->style_sig) {
      overlay->style_sig = sig;
      godot::Ref<godot::Font> font = font_for_css(font_css);
      if (!font.is_null()) overlay->edit->add_theme_font_override("font", font);
      overlay->edit->add_theme_font_size_override("font_size", font_px);
      const godot::Color tint(static_cast<float>(box.r), static_cast<float>(box.g),
                              static_cast<float>(box.b), static_cast<float>(box.a));
      overlay->edit->add_theme_color_override("font_color", tint);
      overlay->edit->add_theme_color_override("font_uneditable_color", tint);
      overlay->edit->add_theme_color_override("font_placeholder_color", tint);
      overlay->edit->add_theme_color_override("caret_color", tint);
      overlay->edit->set_horizontal_alignment(
          box.text_align == 1
              ? godot::HORIZONTAL_ALIGNMENT_CENTER
              : (box.text_align == 2 ? godot::HORIZONTAL_ALIGNMENT_RIGHT
                                     : godot::HORIZONTAL_ALIGNMENT_LEFT));
      overlay->edit->set_max_length(max_len.has_value() ? static_cast<int>(*max_len) : 0);
    }

    const godot::Vector2 p0 = to_canvas.xform(godot::Vector2(static_cast<float>(box.x0),
                                                             static_cast<float>(box.y0)));
    const godot::Vector2 p1 = to_canvas.xform(godot::Vector2(static_cast<float>(box.x1),
                                                             static_cast<float>(box.y1)));
    overlay->edit->set_position(p0);
    overlay->edit->set_size(p1 - p0);

    const std::u16string shown =
        from_utf8(std::string(overlay->edit->get_text().utf8().get_data()));
    const bool edited = shown != overlay->shown;
    overlay->shown = shown;
    const std::u16string source = box.input->text();
    if (shown != source) {
      if (edited) {
        box.input->set_text(shown);
      } else {
        overlay->edit->set_text(godot::String::utf8(to_utf8(source).c_str()));
        overlay->shown = source;
      }
    }
    overlay->synced = box.input->text();

    if (box.input->take_focus_request() && !overlay->edit->has_focus()) {
      overlay->edit->grab_focus();
    }
    if (box.input->take_blur_request() && overlay->edit->has_focus()) {
      overlay->edit->release_focus();
    }

    const bool has_focus = overlay->edit->has_focus();
    if (has_focus && !box.node->focused()) {
      box.node->set_focused(true);
    } else if (!has_focus && overlay->had_focus && box.node->focused()) {
      box.node->set_focused(false);
    }
    overlay->had_focus = has_focus;
  }

  for (Overlay& stale : _overlays) {
    if (stale.edit == nullptr) continue;
    stale.edit->release_focus();
    if (_layer != nullptr) _layer->remove_child(stale.edit);
    stale.edit->queue_free();
  }
  _overlays = std::move(next);
}

void GodotTextInputs::destroy() {
  for (Overlay& overlay : _overlays) {
    if (overlay.edit == nullptr) continue;
    overlay.edit->release_focus();
    if (_layer != nullptr) {
      _layer->remove_child(overlay.edit);
    } else if (_owner != nullptr) {
      _owner->remove_child(overlay.edit);
    }
    overlay.edit->queue_free();
  }
  _overlays.clear();
}

void GodotTextInputs::release_focus() {
  for (Overlay& overlay : _overlays) {
    if (overlay.edit != nullptr && overlay.edit->has_focus()) overlay.edit->release_focus();
  }
}

void GodotTextInputs::release_focus_outside(const godot::Vector2& point) {
  for (Overlay& overlay : _overlays) {
    if (overlay.edit == nullptr || !overlay.edit->has_focus()) continue;
    const godot::Rect2 rect(overlay.edit->get_global_position(), overlay.edit->get_size());
    if (!rect.has_point(point)) overlay.edit->release_focus();
  }
}

GodotTextInputs::Overlay* GodotTextInputs::focused_overlay() {
  for (Overlay& overlay : _overlays) {
    if (overlay.edit != nullptr && overlay.edit->has_focus()) return &overlay;
  }
  return nullptr;
}

bool GodotTextInputs::has_focus() const {
  for (const Overlay& overlay : _overlays) {
    if (overlay.edit != nullptr && overlay.edit->has_focus()) return true;
  }
  return false;
}

void GodotTextInputs::submit_focused() {
  Overlay* const overlay = focused_overlay();
  if (overlay == nullptr || overlay->input == nullptr) return;
  overlay->input->set_text(from_utf8(std::string(overlay->edit->get_text().utf8().get_data())));
  overlay->shown = overlay->input->text();
  overlay->synced = overlay->input->text();
  if (overlay->input->on_submit) overlay->input->on_submit();
}

void GodotTextInputs::cancel_focused() {
  Overlay* const overlay = focused_overlay();
  if (overlay == nullptr || overlay->input == nullptr) return;
  overlay->edit->release_focus();
  if (overlay->input->on_cancel) overlay->input->on_cancel();
}

}
