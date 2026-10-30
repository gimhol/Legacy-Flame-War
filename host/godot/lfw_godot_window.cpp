#include "host/godot/lfw_godot_node.h"
#include "host/godot/lfw_godot_ui_run.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include "host/common/key_code.h"
#include "host/common/lfw_boot.h"
#include "host/common/ui_input_handle.h"
#include "lfw/base/clock.h"
#include "lfw/base/perf.h"
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/cmd.h"
#include "lfw/defines/sync_render_enum.h"
#include "lfw/entity/gen_field.h"
#include "lfw/ui/action/actor.h"
#include "lfw/ui/ui_action_enum.h"
#include "lfw/ui/component/scroll_view.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

#include "host/godot/lfw_godot_node_internal.h"

namespace lfw::host {

namespace {

using godot::String;
using godot::UtilityFunctions;
using godot::Color;
using godot::Rect2;
using godot::Vector2;

using godot_node::from_utf8;
using godot_node::to_utf8;
using godot_node::print_line;
using godot_node::print_error;
using godot_node::wall_now_ms;
using godot_node::is_volume_control;
using godot_node::lower_ascii;
using godot_node::read_file_bytes;
using godot_node::find_repo_root;
using godot_node::exe_dir;
using godot_node::packaged_resources_dir;
using godot_node::user_mods_dir;
using godot_node::user_remote_cache_dir;
using godot_node::pending_custom_game_file;
using godot_node::cwd_string;
using godot_node::titlebar_name;
using godot_node::to_window_edge;
using godot_node::window_state_text;


inline godot::DisplayServer::CursorShape cursor_for_edge(TitleEdge edge) {
  using DS = godot::DisplayServer;
  switch (edge) {
    case TitleEdge::left:
    case TitleEdge::right:
      return DS::CURSOR_HSIZE;
    case TitleEdge::top:
    case TitleEdge::bottom:
      return DS::CURSOR_VSIZE;
    case TitleEdge::top_left:
    case TitleEdge::bottom_right:
      return DS::CURSOR_FDIAGSIZE;
    case TitleEdge::top_right:
    case TitleEdge::bottom_left:
      return DS::CURSOR_BDIAGSIZE;
    case TitleEdge::none:
    default:
      return DS::CURSOR_ARROW;
  }
}

}


void LfwGodot::ui_screen_size(double& w, double& h) const {
  const lfw::host::UiScreenSize s = lfw::host::ui_screen_size(_lfw->world());
  w = s.w;
  h = s.h;
}

void LfwGodot::logical_size(double& w, double& h) const {
  if (_lfw != nullptr) {
    ui_screen_size(w, h);
    return;
  }
  w = static_cast<double>(_logical_w);
  h = static_cast<double>(_logical_h);
}

godot::Rect2 LfwGodot::contain_rect(const godot::Vector2i& window_size) const {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const double vw = static_cast<double>(window_size.x);
  const double vh = static_cast<double>(window_size.y);
  if (!(vw > 0.0) || !(vh > 0.0) || !(lw > 0.0) || !(lh > 0.0)) return godot::Rect2();
  const double aspect_l = lw / lh;
  const double aspect_v = vw / vh;
  const bool same = aspect_l == aspect_v || std::fabs(aspect_l - aspect_v) <
                                                 std::max(1e-5 * std::fabs(aspect_l), 1e-5);
  double sx = vw;
  double sy = vh;
  if (!same) {
    if (aspect_l < aspect_v) {
      sx = vh * aspect_l;
    } else {
      sy = vw / aspect_l;
    }
  }
  // `--integer-scale` 时取**整数倍**：1080p 下的自然比例是 2.4x，非整数放大让同一个
  // 像素在 2px 与 3px 之间跳，卷轴时会抖。取 floor(比例) 后多出来的部分变成黑边，
  // 画面本身是干净的整数放大（默认关，窗口/全屏都保持铺满）。
  if (pixel_integer_scaling()) {
    const double whole = std::floor(std::min(sx / lw, sy / lh));
    if (whole >= 1.0) {
      sx = lw * whole;
      sy = lh * whole;
    }
  }
  sx = std::floor(sx);
  sy = std::floor(sy);
  const double ox = sx < vw ? std::round((vw - sx) * 0.5) : 0.0;
  const double oy = sy < vh ? std::round((vh - sy) * 0.5) : 0.0;
  return godot::Rect2(static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(sx),
                      static_cast<float>(sy));
}

bool LfwGodot::pixel_integer_scaling() const {
  // 显式开关（`--integer-scale`）：像素画在非整数放大下虽然靠最近邻保住了硬边，
  // 但每个源像素落在 2px / 3px 之间来回变，卷轴时会"抖"。开了这个就把内容缩到
  // floor(比例) 的整数倍、多出来的补黑边。默认关，免得全屏时画面突然缩小一圈。
  return _args.integer_scale;
}

godot::Transform2D LfwGodot::contain_transform(const godot::Vector2i& window_size) const {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const godot::Rect2 rect = contain_rect(window_size);
  if (!(lw > 0.0) || !(lh > 0.0) || !(rect.size.x > 0.0f) || !(rect.size.y > 0.0f)) {
    return godot::Transform2D();
  }
  godot::Transform2D xform;
  xform.set_scale(godot::Vector2(static_cast<float>(rect.size.x / lw),
                                 static_cast<float>(rect.size.y / lh)));
  xform.set_origin(godot::Vector2(rect.position));
  return xform;
}

godot::Vector2i LfwGodot::window_size_px() const {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (_windowed && ds != nullptr && ds->window_get_size() != godot::Vector2i()) {
    return ds->window_get_size();
  }
  const godot::Viewport* const vp = get_viewport();
  return vp != nullptr ? godot::Vector2i(vp->get_visible_rect().size) : godot::Vector2i();
}

godot::Transform2D LfwGodot::final_transform() const {
  const godot::Viewport* const vp = get_viewport();
  return vp != nullptr ? vp->get_final_transform() : godot::Transform2D();
}

void LfwGodot::apply_game_clip(const godot::Vector2i& window_size) {
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs == nullptr || !_windowed) return;
  const godot::RID item = get_canvas_item();
  rs->canvas_item_set_custom_rect(item, true,
                                  final_transform().affine_inverse().xform(contain_rect(window_size)));
  rs->canvas_item_set_clip(item, true);
}

void LfwGodot::apply_font_oversampling(const godot::Transform2D& contain) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  const godot::Vector2 scale = contain.get_scale();
  const double over = std::max(static_cast<double>(scale.x), static_cast<double>(scale.y));
  if (!(over > 0.0)) return;
  vp->set_oversampling_override(static_cast<float>(over));
}

void LfwGodot::report_stretch(const godot::Transform2D& contain, const godot::Rect2& content) {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  const godot::Vector2 scale = contain.get_scale();
  const double over = std::max(static_cast<double>(scale.x), static_cast<double>(scale.y));
  const godot::Vector2i vp_size = window_size_px();
  const godot::Transform2D final = final_transform();
  const double screen_scale = titlebar_display_scale();
  double content_scale = 1.0;
  if (godot::Viewport* const vp = get_viewport()) {
    if (const godot::Window* const win = godot::Object::cast_to<godot::Window>(vp)) {
      content_scale = static_cast<double>(win->get_content_scale_factor());
    }
  }
  char buf[448];
  std::snprintf(buf, sizeof(buf),
                "[godot] stretch: mode=disabled logical=%.0fx%.0f content=%.0fx%.0f+%.0f+%.0f "
                "scale=%.5fx%.5f oversampling=%.3f viewport=%dx%d final=%.5fx%.5f "
                "screen_scale=%.3f content_scale=%.3f",
                lw, lh, content.size.x, content.size.y, content.position.x, content.position.y,
                scale.x, scale.y, over, vp_size.x, vp_size.y, final.get_scale().x,
                final.get_scale().y, screen_scale, content_scale);
  const std::string line(buf);
  if (line == _stretch_report) return;
  _stretch_report = line;
  print_line(line);
}

void LfwGodot::apply_window_setup() {
  using godot::DisplayServer;
  DisplayServer* const ds = DisplayServer::get_singleton();
  if (ds == nullptr) return;
  const std::string display(ds->get_name().utf8().get_data());
  if (display == "headless") {
    print_line("[godot] window: headless display server (no window, no titlebar)");
    return;
  }
  _windowed = true;
  void* frame_hwnd = nullptr;
  const bool borderless =
      _args.titlebar == TitlebarMode::none || _args.titlebar == TitlebarMode::custom;
  if (borderless) {
    ds->window_set_flag(DisplayServer::WINDOW_FLAG_BORDERLESS, true);
  } else if (_args.titlebar == TitlebarMode::frame) {
    frame_hwnd = reinterpret_cast<void*>(
        static_cast<uintptr_t>(ds->window_get_native_handle(DisplayServer::WINDOW_HANDLE)));
    _native_frame = install_native_frame(frame_hwnd);
    const std::string evidence = native_frame_evidence(frame_hwnd);
    print_line(std::string("[godot] titlebar: frame -> native frame (captionless) ") +
               (evidence.empty() ? std::string("hwnd=-") : evidence) +
               (_native_frame ? " (WM_NCCALCSIZE=0)"
                              : " (NOT installed: no HWND or no WS_THICKFRAME)"));
  }
  const godot::Vector2i logical(_logical_w, _logical_h);
  const double scale = _args.window_scale;
  const bool explicit_size = _args.window_w > 0 && _args.window_h > 0;
  const godot::Vector2i want =
      explicit_size
          ? godot::Vector2i(_args.window_w, _args.window_h)
          : godot::Vector2i(static_cast<int>(std::lround(static_cast<double>(logical.x) * scale)),
                            static_cast<int>(std::lround(static_cast<double>(logical.y) * scale)));
  ds->window_set_flag(DisplayServer::WINDOW_FLAG_RESIZE_DISABLED, false);
  ds->window_set_min_size(logical);
  int deco_x = 0;
  int deco_y = 0;
  int in_l = 0;
  int in_t = 0;
  int in_r = 0;
  int in_b = 0;
  if (_native_frame) {
    native_frame_decoration(frame_hwnd, &deco_x, &deco_y);
    native_frame_insets(frame_hwnd, &in_l, &in_t, &in_r, &in_b);
  }
  const godot::Vector2i request =
      _native_frame ? godot::Vector2i(want.x - deco_x + in_l + in_r, want.y - deco_y + in_t + in_b)
                    : want;
  if (_native_frame) {
    char req_buf[192];
    std::snprintf(req_buf, sizeof(req_buf),
                  "[godot] window: native frame -> request %dx%d (want %dx%d, decoration %dx%d, "
                  "kept frame %dx%d)",
                  request.x, request.y, want.x, want.y, deco_x, deco_y, in_l + in_r, in_t + in_b);
    print_line(req_buf);
  }
  if (ds->window_get_size() != want) {
    ds->window_set_size(request);
    if (scale != 1.0 || explicit_size) {
      const int screen = ds->window_get_current_screen();
      const godot::Vector2i screen_pos = ds->screen_get_position(screen);
      const godot::Vector2i screen_size = ds->screen_get_size(screen);
      ds->window_set_position(screen_pos + (screen_size - want) / 2);
    }
  }
  const godot::Vector2i now = ds->window_get_size();
  const godot::Vector2i deco = ds->window_get_size_with_decorations();
  const godot::Vector2i pos = ds->window_get_position();
  const godot::Transform2D contain = contain_transform(now);
  const godot::Rect2 content = contain_rect(now);
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] window: display=%s titlebar=%s scale=%.2f borderless=%d frame=%d "
                "size=%dx%d deco=%dx%d pos=%d,%d mode=%d logical=%dx%d resizable=%d min=%dx%d "
                "content=%.0fx%.0f+%.0f+%.0f",
                display.c_str(), titlebar_name(_args.titlebar), scale,
                ds->window_get_flag(DisplayServer::WINDOW_FLAG_BORDERLESS) ? 1 : 0,
                _native_frame ? 1 : 0, now.x,
                now.y, deco.x, deco.y, pos.x, pos.y, static_cast<int>(ds->window_get_mode()),
                logical.x, logical.y,
                ds->window_get_flag(DisplayServer::WINDOW_FLAG_RESIZE_DISABLED) ? 0 : 1,
                ds->window_get_min_size().x, ds->window_get_min_size().y, content.size.x,
                content.size.y, content.position.x, content.position.y);
  print_line(buf);
  report_stretch(contain, content);
}

bool LfwGodot::titlebar_visible() const {
  if (!_windowed || !titlebar_drawn()) return false;
  return godot::DisplayServer::get_singleton() != nullptr;
}


lfw::host::TitleBarConditions LfwGodot::titlebar_conditions() const {
  lfw::host::TitleBarConditions c;
  if (_lfw == nullptr) return c;
  const lfw::World& world = _lfw->world();
  c.cheat_gim_ink = _lfw->is_cheat(lfw::cmd::kGIM_INK);
  lfw::ui::UILayers* const layers = _lfw->ui_layers();
  const lfw::ui::UINode* const ui = layers != nullptr ? layers->ui() : nullptr;
  c.has_ui = ui != nullptr;
  c.settings_page = ui != nullptr && lfw::to_string(ui->id()) == u"settings";
  const lfw::ui::UILayer* const bottom = layers != nullptr ? layers->bottom() : nullptr;
  c.bottom_pages = bottom != nullptr ? static_cast<int>(bottom->pages().size()) : 0;
  if (const lfw::Background* const bg = world.bg()) {
    c.bg_is_void = lfw::to_string(bg->id()) == u"VOID_BG";
  }
  c.first_page_init_demo = _lfw->first_page == u"init_demo";
  c.has_window = _windowed;
  c.mobile_or_toy = false;
  if (godot::DisplayServer* const ds = godot::DisplayServer::get_singleton()) {
    const godot::DisplayServer::WindowMode mode = ds->window_get_mode();
    c.fullscreen = mode == godot::DisplayServer::WINDOW_MODE_FULLSCREEN ||
                   mode == godot::DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
  }
  return c;
}

bool LfwGodot::titlebar_checked(lfw::host::TitleAction action) const {
  using lfw::host::TitleAction;
  if (_lfw == nullptr) return false;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const godot::DisplayServer::WindowMode mode =
      ds != nullptr ? ds->window_get_mode() : godot::DisplayServer::WINDOW_MODE_WINDOWED;
  switch (action) {
    case TitleAction::bgm_mute:
      return _lfw->sounds_bgm_muted();
    case TitleAction::sound_mute:
      return _lfw->sounds_sound_muted();
    case TitleAction::f1:
      return _lfw->world().paused();
    case TitleAction::f5:
      return _lfw->world().dataset.playrate() != 1.0;
    case TitleAction::fullscreen:
      return mode == godot::DisplayServer::WINDOW_MODE_FULLSCREEN ||
             mode == godot::DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
    case TitleAction::maximize:
      return native_frame() ? mode == godot::DisplayServer::WINDOW_MODE_MAXIMIZED : _maximized;
    case TitleAction::dev_ui:
    case TitleAction::f4:
    case TitleAction::settings:
    case TitleAction::minimize:
    case TitleAction::close:
    case TitleAction::none:
      break;
  }
  return false;
}

void LfwGodot::titlebar_activate(int index) {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  switch (action) {
    case TitleAction::dev_ui:
      print_line("[godot] titlebar: dev-ui (no dev panel in this host)");
      break;
    case TitleAction::f4:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF4)});
      print_line("[godot] titlebar: f4 pages=" + ui_pages_text());
      break;
    case TitleAction::f1:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF1)});
      print_line("[godot] titlebar: f1 pages=" + ui_pages_text());
      break;
    case TitleAction::f5:
      _lfw->push_cmd({std::u16string(lfw::cmd::kF5)});
      print_line("[godot] titlebar: f5 pages=" + ui_pages_text());
      break;
    case TitleAction::settings: {
      _lfw->push_cmd({std::u16string(lfw::cmd::kF2)});
      lfw::ui::UILayers* const layers = _lfw->ui_layers();
      lfw::ui::UINode* const ui = layers != nullptr ? layers->ui() : nullptr;
      lfw::ui::UILayer* const layer = ui != nullptr ? ui->layer() : nullptr;
      if (layer != nullptr && lfw::to_string(ui->id()) == u"settings") {
        layer->pop(lfw::ui::UIPopPageOpts{});
        print_line("[godot] titlebar: settings (pop) pages=" + ui_pages_text());
      } else if (layers != nullptr) {
        auto opts = std::make_shared<lfw::Object>();
        opts->set(u"id", Value(std::u16string(u"settings")));
        layers->set_page(Value(std::move(opts)), 1.0);
        print_line("[godot] titlebar: settings (set_page) pages=" + ui_pages_text());
      }
      break;
    }
    case TitleAction::bgm_mute: {
      const bool muted = !_lfw->sounds_bgm_muted();
      _lfw->sounds_set_bgm_muted(muted);
      print_line(std::string("[godot] titlebar: bgm_muted=") + (muted ? "true" : "false"));
      break;
    }
    case TitleAction::sound_mute: {
      const bool muted = !_lfw->sounds_sound_muted();
      _lfw->sounds_set_sound_muted(muted);
      print_line(std::string("[godot] titlebar: sound_muted=") + (muted ? "true" : "false"));
      break;
    }
    case TitleAction::fullscreen: {
      if (ds == nullptr) break;
      const bool full = titlebar_checked(TitleAction::fullscreen);
      ds->window_set_mode(full ? godot::DisplayServer::WINDOW_MODE_WINDOWED
                               : godot::DisplayServer::WINDOW_MODE_FULLSCREEN);
      if (full && _maximized) {
        const godot::Rect2i usable = ds->screen_get_usable_rect(ds->window_get_current_screen());
        ds->window_set_position(usable.position);
        ds->window_set_size(usable.size);
      }
      print_line(std::string("[godot] titlebar: fullscreen=") + (full ? "false" : "true"));
      break;
    }
    case TitleAction::minimize:
      if (ds == nullptr) break;
      ds->window_set_mode(godot::DisplayServer::WINDOW_MODE_MINIMIZED);
      print_line("[godot] titlebar: minimize (" + window_state_text(ds) + ")");
      break;
    case TitleAction::maximize: {
      if (ds == nullptr) break;
      if (native_frame()) {
        const bool maxed = ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED;
        ds->window_set_mode(maxed ? godot::DisplayServer::WINDOW_MODE_WINDOWED
                                  : godot::DisplayServer::WINDOW_MODE_MAXIMIZED);
        print_line(std::string("[godot] titlebar: ") + (maxed ? "restore" : "maximize") +
                   " (native frame) (" + window_state_text(ds) + ")");
        break;
      }
      if (_maximized) {
        if (ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED) {
          ds->window_set_mode(godot::DisplayServer::WINDOW_MODE_WINDOWED);
        }
        if (_restore_size.x > 0 && _restore_size.y > 0) {
          ds->window_set_size(_restore_size);
          ds->window_set_position(_restore_pos);
        }
        _maximized = false;
        print_line("[godot] titlebar: restore (" + window_state_text(ds) + ")");
      } else {
        _restore_size = ds->window_get_size();
        _restore_pos = ds->window_get_position();
        const godot::Rect2i usable = ds->screen_get_usable_rect(ds->window_get_current_screen());
        ds->window_set_position(usable.position);
        ds->window_set_size(usable.size);
        _maximized = true;
        print_line("[godot] titlebar: maximize (" + window_state_text(ds) + ")");
      }
      break;
    }
    case TitleAction::close:
      print_line("[godot] titlebar: close");
      print_exit_summary();
      quit_with(0);
      break;
    case TitleAction::none:
      break;
  }
  queue_redraw();
}

void LfwGodot::titlebar_update_hover(const lfw::host::TitleBarLayout& layout,
                                     const lfw::host::TitleHit& hit, double bx, double by) {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleHitKind;
  const int hover = hit.kind == TitleHitKind::button ? hit.index : -1;
  int popup = -1;
  if (_bar.slider_drag >= 0) {
    popup = _bar.slider_drag;
  } else {
    for (int i = 0; i < TitleBarMetrics::count; ++i) {
      if (!layout.visible[i] || !is_volume_control(i)) continue;
      const bool open = _bar.popup == i;
      if (hover == i ||
          (open && lfw::host::titlebar_rect_contains(lfw::host::titlebar_popup_rect(layout.rect[i]),
                                                     bx, by))) {
        popup = i;
      }
    }
  }
  const bool drag_hover = hit.kind == TitleHitKind::drag;
  if (hover == _bar.hover && drag_hover == _bar.drag_hover && popup == _bar.popup) return;
  _bar.hover = hover;
  _bar.drag_hover = drag_hover;
  _bar.popup = popup;
  queue_redraw();
}

void LfwGodot::titlebar_set_volume(int index, double v) {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return;
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  if (action == TitleAction::bgm_mute) {
    _lfw->sounds_set_bgm_volume(v);
    if (v > 0.0 && _lfw->sounds_bgm_muted()) _lfw->sounds_set_bgm_muted(false);
  } else if (action == TitleAction::sound_mute) {
    _lfw->sounds_set_sound_volume(v);
    if (v > 0.0 && _lfw->sounds_sound_muted()) _lfw->sounds_set_sound_muted(false);
  } else {
    return;
  }
  queue_redraw();
}

double LfwGodot::titlebar_volume(int index) const {
  using lfw::host::TitleAction;
  const auto& list = lfw::host::titlebar_controls();
  if (_lfw == nullptr || index < 0 || index >= static_cast<int>(list.size())) return 0.0;
  const TitleAction action = list[static_cast<std::size_t>(index)].action;
  if (action == TitleAction::bgm_mute) return _lfw->sounds_bgm_volume();
  if (action == TitleAction::sound_mute) return _lfw->sounds_sound_volume();
  return 0.0;
}

double LfwGodot::titlebar_window_width(const godot::Vector2i& window_size) const {
  return static_cast<double>(window_size.x) / titlebar_display_scale();
}

double LfwGodot::titlebar_display_scale() const {
  if (_args.display_scale > 0.0) return _args.display_scale;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr || !_windowed) return 1.0;
  const double s = ds->screen_get_scale(godot::DisplayServer::SCREEN_OF_MAIN_WINDOW);
  return s > 0.0 ? s : 1.0;
}

void LfwGodot::report_titlebar_controls() {
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return;
  const godot::Vector2i win_size = ds->window_get_size();
  const double bar_w = titlebar_window_width(win_size);
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, titlebar_conditions());
  const auto& list = lfw::host::titlebar_controls();
  std::string text;
  for (int i = 0; i < lfw::host::TitleBarMetrics::count; ++i) {
    if (!layout.visible[i]) continue;
    if (!text.empty()) text += " ";
    text += lfw::host::titlebar_action_name(
        list[static_cast<std::size_t>(i)].action);
  }
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] titlebar: icons=%s bar=%.0fx%.0f icon=%.1f scale=%.3f controls(%d)=%s",
                _icons_dir.empty() ? "<none>" : _icons_dir.c_str(), bar_w,
                lfw::host::TitleBarMetrics::bar_height,
                lfw::host::TitleBarMetrics::button * lfw::host::TitleBarMetrics::icon_scale,
                titlebar_display_scale(), layout.count, text.c_str());
  const std::string line(buf);
  if (line == _icon_report) return;
  _icon_report = line;
  print_line(line);
}

bool LfwGodot::titlebar_consume(const godot::Ref<godot::InputEvent>& event) {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleHit;
  using lfw::host::TitleHitKind;
  if (!titlebar_visible()) return false;
  LfwPointingEvent e;
  const PointerKind kind = to_pointing_event(event, e);
  if (kind == PointerKind::none) return false;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr) return false;
  const godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return false;

  const godot::Transform2D final = vp->get_final_transform();
  const godot::Vector2 win =
      final.xform(godot::Vector2(static_cast<float>(e.x), static_cast<float>(e.y)));
  const double scale = titlebar_display_scale();
  const godot::Vector2i win_size = ds->window_get_size();
  const double bx = static_cast<double>(win.x) / scale;
  const double by = static_cast<double>(win.y) / scale;
  const double win_w = static_cast<double>(win_size.x) / scale;
  const double win_h = static_cast<double>(win_size.y) / scale;
  const double bar_w = titlebar_window_width(win_size);
  const lfw::host::TitleBarConditions cond = titlebar_conditions();
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, cond);
  report_titlebar_controls();

  const bool popup_open = _bar.popup >= 0 && layout.visible[_bar.popup];
  const lfw::host::TitleBarRect popup_rect =
      popup_open ? lfw::host::titlebar_popup_rect(layout.rect[_bar.popup])
                 : lfw::host::TitleBarRect{};
  const TitleHit hit = lfw::host::titlebar_hit(bx, by, layout);
  const bool in_popup = popup_open && lfw::host::titlebar_rect_contains(popup_rect, bx, by);

  if (kind == PointerKind::move || kind == PointerKind::down) {
    titlebar_update_hover(layout, hit, bx, by);
  }

  if (kind == PointerKind::move) {
    using DS = godot::DisplayServer;
    DS::CursorShape shape =
        native_frame() ? DS::CURSOR_ARROW
                       : cursor_for_edge(titlebar_edge_at(bx, by, win_w, win_h));
    const bool on_volume = in_popup || (hit.kind == TitleHitKind::button &&
                                        is_volume_control(hit.index));
    if (shape == DS::CURSOR_ARROW && on_volume) shape = DS::CURSOR_POINTING_HAND;
    if (static_cast<int>(shape) != _cursor_shape) {
      _cursor_shape = static_cast<int>(shape);
      ds->cursor_set_shape(shape);
    }
  }

  if (!native_frame() && kind == PointerKind::down && e.button == 0.0 &&
      ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_WINDOWED &&
      !ds->window_get_flag(godot::DisplayServer::WINDOW_FLAG_RESIZE_DISABLED)) {
    const TitleEdge edge = titlebar_edge_at(bx, by, win_w, win_h);
    if (edge != TitleEdge::none) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "[godot] titlebar: resize edge=%d at=(%.0f, %.0f)",
                    static_cast<int>(edge), bx, by);
      print_line(buf);
      _bar_capture = true;
      ds->window_start_resize(to_window_edge(edge));
      return true;
    }
  }

  if (kind == PointerKind::move) {
    if (_bar.slider_drag >= 0) {
      lfw::host::TitleBarRect drag_popup;
      lfw::host::TitleBarRect drag_track;
      lfw::host::titlebar_popup_rects(layout.rect[_bar.slider_drag], drag_popup, drag_track);
      titlebar_set_volume(_bar.slider_drag, lfw::host::titlebar_slider_value(drag_track, by));
      return true;
    }
    if (hit.kind == TitleHitKind::none && !in_popup) return false;
    return true;
  }

  if (kind == PointerKind::down) {
    lfw::host::TitleBarRect down_popup;
    lfw::host::TitleBarRect down_track;
    bool down_in_popup = false;
    if (_bar.popup >= 0 && layout.visible[_bar.popup]) {
      lfw::host::titlebar_popup_rects(layout.rect[_bar.popup], down_popup, down_track);
      down_in_popup = lfw::host::titlebar_rect_contains(down_popup, bx, by);
    }
    if (e.button != 0.0) {
      return hit.kind != TitleHitKind::none || down_in_popup;
    }
    if (down_in_popup) {
      _bar_capture = true;
      titlebar_set_volume(_bar.popup, lfw::host::titlebar_slider_value(down_track, by));
      _bar.slider_drag = _bar.popup;
      return true;
    }
    if (hit.kind == TitleHitKind::button) {
      _bar_capture = true;
      titlebar_activate(hit.index);
      return true;
    }
    if (hit.kind == TitleHitKind::drag) {
      _bar_capture = true;
      const bool maxed =
          _maximized || ds->window_get_mode() == godot::DisplayServer::WINDOW_MODE_MAXIMIZED;
      bool dbl = false;
      if (const godot::InputEventMouseButton* const mb =
              godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());
          mb != nullptr) {
        dbl = mb->is_double_click();
      }
      if (dbl && (native_frame() || maxed)) {
        const auto& list = lfw::host::titlebar_controls();
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
          if (list[static_cast<std::size_t>(i)].action == lfw::host::TitleAction::maximize) {
            titlebar_activate(i);
            break;
          }
        }
        return true;
      }
      if (maxed) {
        print_line("[godot] titlebar: drag ignored (maximized)");
        return true;
      }
      if (!_bar_dragging) {
        _bar_dragging = true;
        print_line("[godot] titlebar: drag");
        ds->window_start_drag();
        _bar_dragging = false;
      }
      return true;
    }
    return false;
  }

  if (kind == PointerKind::up) {
    if (_bar.slider_drag >= 0) {
      _bar.slider_drag = -1;
      _bar_capture = false;
      return true;
    }
    if (hit.kind != TitleHitKind::none || in_popup) {
      _bar_capture = false;
      return true;
    }
  }
  if (kind == PointerKind::wheel) return hit.kind != TitleHitKind::none || in_popup;
  if (in_popup) return true;
  if (_bar_capture) {
    if (kind == PointerKind::up) _bar_capture = false;
    return true;
  }
  return false;
}

void LfwGodot::setup_icons() {
  std::vector<std::filesystem::path> probes;
  if (_args.icons_given) probes.push_back(_args.icons);
  // 打包版：优先 `<exe>/resources/icons`（它才是随包发的，别被仓库里的 src/assets 抢走）
  if (const std::filesystem::path res = packaged_resources_dir(); !res.empty())
    probes.push_back(res / "icons");
  std::error_code ec;
  probes.push_back(std::filesystem::current_path(ec));
  if (godot::OS* const os = godot::OS::get_singleton()) {
    probes.push_back(std::filesystem::path(os->get_executable_path().utf8().get_data())
                         .parent_path());
  }
  for (const std::filesystem::path& probe : probes) {
    if (probe.empty()) continue;
    std::error_code stat_ec;
    if (std::filesystem::is_regular_file(probe / "btn_1_1.png", stat_ec)) {
      _icons_dir = probe.generic_string();
    } else {
      const std::filesystem::path root = find_repo_root(probe);
      if (!root.empty()) _icons_dir = (root / "src" / "assets").generic_string();
    }
    if (!_icons_dir.empty()) break;
  }
  if (_icons_dir.empty()) {
    print_error("[godot] titlebar icons: no btn_1_1.png (给 `--icons <dir>`；条上只画命中盒)");
    return;
  }
  const std::string dir = _icons_dir;
  _icons = std::make_unique<GodotTextureCache>(
      [dir](const std::string& name, std::string& out) {
        return read_file_bytes(std::filesystem::path(dir) / name, out);
      });
  print_line("[godot] titlebar icons: " + dir);
}

void LfwGodot::apply_window_icon() {
  std::vector<std::filesystem::path> dirs;
  if (!_icons_dir.empty()) dirs.emplace_back(_icons_dir);
  if (const std::filesystem::path res = packaged_resources_dir(); !res.empty())
    dirs.push_back(res / "icons");
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  if (ds == nullptr || dirs.empty()) return;
  const int64_t handle = ds->window_get_native_handle(godot::DisplayServer::WINDOW_HANDLE, 0);
  std::error_code ec;

  for (const std::filesystem::path& dir : dirs) {
    // 1) `icon.ico`：Godot 的 Image 不认 ICO，直接 Win32 `LoadImage` + `WM_SETICON`
    const std::filesystem::path ico = dir / "icon.ico";
    if (handle != 0 && std::filesystem::is_regular_file(ico, ec) &&
        lfw::host::set_window_icon(reinterpret_cast<void*>(static_cast<intptr_t>(handle)),
                                   ico.generic_string())) {
      print_line("[godot] window icon: " + ico.generic_string());
      return;
    }
    // 2) `icon.png`：走 Godot 的 set_icon
    const std::filesystem::path png = dir / "icon.png";
    std::string raw;
    if (!read_file_bytes(png, raw)) continue;
    godot::PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(raw.size()));
    std::memcpy(bytes.ptrw(), raw.data(), raw.size());
    godot::Ref<godot::Image> icon;
    icon.instantiate();
    if (icon->load_png_from_buffer(bytes) != godot::OK) {
      print_error("[godot] window icon: decode failed: " + png.generic_string());
      continue;
    }
    ds->set_icon(icon);
    print_line("[godot] window icon: " + png.generic_string());
    return;
  }
  // 两个都没有就保持 Godot 默认图标
}

void LfwGodot::draw_titlebar(godot::CanvasItem& canvas) {
  using lfw::host::TitleBarMetrics;
  using lfw::host::TitleBarRect;
  godot::DisplayServer* const ds = godot::DisplayServer::get_singleton();
  const godot::Viewport* const vp = get_viewport();
  if (ds == nullptr || vp == nullptr) return;
  const godot::Transform2D final = vp->get_final_transform();
  const double bar_w = titlebar_window_width(ds->window_get_size());
  const lfw::host::TitleBarLayout layout = lfw::host::titlebar_layout(bar_w, titlebar_conditions());
  if (!(bar_w > 0.0)) return;
  report_titlebar_controls();
  godot::Transform2D css_to_canvas;
  css_to_canvas.set_scale(godot::Vector2(static_cast<float>(titlebar_display_scale()),
                                         static_cast<float>(titlebar_display_scale())));
  canvas.draw_set_transform_matrix(final.affine_inverse() * css_to_canvas);

  const float drag_h = static_cast<float>(TitleBarMetrics::drag_height);
  if (_bar.drag_hover) {
    canvas.draw_rect(Rect2(0.0f, 0.0f, static_cast<float>(bar_w), drag_h),
              Color(0x88 / 255.0f, 0x88 / 255.0f, 0x88 / 255.0f, 0x55 / 255.0f), true);
  }

  const auto& list = lfw::host::titlebar_controls();
  for (int i = 0; i < TitleBarMetrics::count; ++i) {
    if (!layout.visible[i]) continue;
    const lfw::host::TitleBarControl& control = list[static_cast<std::size_t>(i)];
    const bool checked = titlebar_checked(control.action);
    const TitleBarRect icon = lfw::host::titlebar_icon_rect(layout.rect[i], _bar.hover == i);
    godot::Ref<godot::ImageTexture> tex;
    if (_icons != nullptr) tex = _icons->get(lfw::host::titlebar_control_image(control, checked));
    if (tex.is_null()) {
      canvas.draw_rect(Rect2(static_cast<float>(icon.x), static_cast<float>(icon.y),
                      static_cast<float>(icon.w), static_cast<float>(icon.h)),
                Color(0.85f, 0.88f, 0.95f, 0.18f), true);
      continue;
    }
    canvas.draw_texture_rect(tex,
                      Rect2(static_cast<float>(icon.x), static_cast<float>(icon.y),
                            static_cast<float>(icon.w), static_cast<float>(icon.h)),
                      false, Color(1.0f, 1.0f, 1.0f, 1.0f));
  }

  if (_bar.popup >= 0 && layout.visible[_bar.popup]) {
    TitleBarRect popup;
    TitleBarRect track;
    lfw::host::titlebar_popup_rects(layout.rect[_bar.popup], popup, track);
    const double v = titlebar_volume(_bar.popup);
    const double clamped = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    draw_rounded_rect(canvas, popup, TitleBarMetrics::popup_radius, Color(0x0F / 255.0f, 0x18 / 255.0f,
                                                                  0x30 / 255.0f, 0xCC / 255.0f));
    draw_rounded_rect(canvas, track, TitleBarMetrics::slider_w * 0.5, Color(0.0f, 0.0f, 0.0f, 0x88 / 255.0f));
    draw_rounded_rect(canvas, lfw::host::titlebar_fill_rect(track, clamped), TitleBarMetrics::slider_w * 0.5,
                      Color(0x5A / 255.0f, 0x88 / 255.0f, 0xE0 / 255.0f, 1.0f));
    const TitleBarRect knob = lfw::host::titlebar_knob_rect(track, clamped);
    canvas.draw_circle(Vector2(static_cast<float>(knob.x), static_cast<float>(knob.y)),
                static_cast<float>(knob.w * 0.5), Color(1.0f, 1.0f, 1.0f, 1.0f));
  }
  canvas.draw_set_transform_matrix(godot::Transform2D());
}

void LfwGodot::draw_clip_probe(const godot::Transform2D& contain) {
  double lw = 0.0;
  double lh = 0.0;
  logical_size(lw, lh);
  if (!(lw > 0.0) || !(lh > 0.0)) return;
  const float w = static_cast<float>(lw);
  const float h = static_cast<float>(lh);
  const float b = 8.0f;
  const godot::Color color(1.0f, 0.0f, 1.0f, 0.85f);
  draw_set_transform_matrix(contain);
  draw_rect(Rect2(-b, -b, w + 2.0f * b, 2.0f * b), color, true);
  draw_rect(Rect2(-b, h - b, w + 2.0f * b, 2.0f * b), color, true);
  draw_rect(Rect2(-b, -b, 2.0f * b, h + 2.0f * b), color, true);
  draw_rect(Rect2(w - b, -b, 2.0f * b, h + 2.0f * b), color, true);
  draw_set_transform_matrix(godot::Transform2D());
}

void LfwGodot::draw_rounded_rect(godot::CanvasItem& canvas, const lfw::host::TitleBarRect& r, double radius,
                                 const godot::Color& color) {
  const double limit = r.w < r.h ? r.w : r.h;
  const float rad = static_cast<float>(radius > limit * 0.5 ? limit * 0.5 : radius);
  if (!(rad > 0.0f)) {
    canvas.draw_rect(Rect2(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w),
                    static_cast<float>(r.h)),
              color, true);
    return;
  }
  const float x = static_cast<float>(r.x);
  const float y = static_cast<float>(r.y);
  const float w = static_cast<float>(r.w);
  const float h = static_cast<float>(r.h);
  canvas.draw_rect(Rect2(x + rad, y, w - 2.0f * rad, h), color, true);
  canvas.draw_rect(Rect2(x, y + rad, w, h - 2.0f * rad), color, true);
  canvas.draw_circle(Vector2(x + rad, y + rad), rad, color);
  canvas.draw_circle(Vector2(x + w - rad, y + rad), rad, color);
  canvas.draw_circle(Vector2(x + rad, y + h - rad), rad, color);
  canvas.draw_circle(Vector2(x + w - rad, y + h - rad), rad, color);
}

void LfwGodot::draw_titlebar_into(godot::CanvasItem& canvas) {
  if (!titlebar_visible()) return;
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), true);
  draw_titlebar(canvas);
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), false);
}

}
