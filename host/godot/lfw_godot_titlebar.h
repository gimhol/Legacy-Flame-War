#pragma once

#include <array>

namespace lfw::host {

enum class TitleAction {
  none,
  dev_ui,
  f4,
  bgm_mute,
  sound_mute,
  f1,
  f5,
  settings,
  fullscreen,
  minimize,
  maximize,
  close
};

enum class TitleEdge {
  none,
  top_left,
  top,
  top_right,
  left,
  right,
  bottom_left,
  bottom,
  bottom_right
};

struct TitleBarRect {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

struct TitleBarMetrics {
  static constexpr double bar_height = 32.0;
  static constexpr double button = 32.0;
  static constexpr double icon_scale = 0.6;
  static constexpr double icon_scale_hover = 0.8;
  static constexpr double drag_height = 28.8;
  static constexpr double resize_band = 6.0;
  static constexpr int count = 11;
  static constexpr double popup_outer_pad = 2.0;
  static constexpr double popup_pad_x = 10.0;
  static constexpr double popup_pad_top = 8.0;
  static constexpr double popup_pad_bottom = 10.0;
  static constexpr double popup_radius = 8.0;
  static constexpr double slider_w = 6.0;
  static constexpr double slider_h = 84.0;
  static constexpr double knob = 14.0;
  static constexpr double popup_w = 2.0 * popup_pad_x + slider_w;
  static constexpr double popup_h = popup_outer_pad + popup_pad_top + slider_h +
                                    popup_pad_bottom;
};

struct TitleBarControl {
  TitleAction action = TitleAction::none;
  const char* img0 = "";
  const char* img1 = "";
};

inline const char* titlebar_control_image(const TitleBarControl& c, bool checked) {
  if (checked) return c.img1[0] != '\0' ? c.img1 : c.img0;
  return c.img0;
}

inline const std::array<TitleBarControl, TitleBarMetrics::count>& titlebar_controls() {
  static const std::array<TitleBarControl, TitleBarMetrics::count> kList = {{
      {TitleAction::dev_ui, "btn_1_2.png", "btn_1_3.png"},
      {TitleAction::f4, "btn_2_3.png", "btn_2_3.png"},
      {TitleAction::bgm_mute, "btn_2_0.png", "btn_3_0.png"},
      {TitleAction::sound_mute, "btn_0_3.png", "btn_1_0.png"},
      {TitleAction::f1, "btn_2_1.png", "btn_2_2.png"},
      {TitleAction::f5, "btn_4_3.png", "btn_4_3.png"},
      {TitleAction::settings, "btn_1_1.png", "btn_1_1.png"},
      {TitleAction::fullscreen, "btn_3_1.png", "btn_3_2.png"},
      {TitleAction::minimize, "btn_0_4.png", "btn_0_4.png"},
      {TitleAction::maximize, "btn_1_4.png", "btn_2_4.png"},
      {TitleAction::close, "btn_3_3.png", "btn_3_3.png"},
  }};
  return kList;
}

inline const char* titlebar_action_name(TitleAction action) {
  switch (action) {
    case TitleAction::dev_ui:
      return "dev_ui";
    case TitleAction::f4:
      return "f4";
    case TitleAction::bgm_mute:
      return "bgm";
    case TitleAction::sound_mute:
      return "sound";
    case TitleAction::f1:
      return "f1";
    case TitleAction::f5:
      return "f5";
    case TitleAction::settings:
      return "settings";
    case TitleAction::fullscreen:
      return "fullscreen";
    case TitleAction::minimize:
      return "minimize";
    case TitleAction::maximize:
      return "maximize";
    case TitleAction::close:
      return "close";
    case TitleAction::none:
      break;
  }
  return "-";
}

struct TitleBarConditions {
  bool cheat_gim_ink = false;
  bool has_ui = false;
  int bottom_pages = 0;
  bool bg_is_void = true;
  bool settings_page = false;
  bool first_page_init_demo = false;
  bool fullscreen = false;
  bool has_window = false;
  bool mobile_or_toy = false;
};

inline bool titlebar_control_visible(TitleAction action, const TitleBarConditions& c) {
  switch (action) {
    case TitleAction::dev_ui:
      return c.cheat_gim_ink;
    case TitleAction::f4:
      return c.has_ui && c.bottom_pages > 1;
    case TitleAction::bgm_mute:
    case TitleAction::sound_mute:
      return true;
    case TitleAction::f1:
      return !c.bg_is_void && !c.settings_page;
    case TitleAction::f5:
      return !c.bg_is_void && !c.settings_page && c.first_page_init_demo;
    case TitleAction::settings:
      return true;
    case TitleAction::fullscreen:
      return !c.mobile_or_toy && !c.first_page_init_demo;
    case TitleAction::minimize:
    case TitleAction::maximize:
    case TitleAction::close:
      return !c.fullscreen && c.has_window;
    case TitleAction::none:
      break;
  }
  return false;
}

struct TitleBarLayout {
  bool visible[TitleBarMetrics::count] = {};
  TitleBarRect rect[TitleBarMetrics::count] = {};
  int count = 0;
};

inline TitleBarLayout titlebar_layout(double window_w, const TitleBarConditions& c) {
  const auto& list = titlebar_controls();
  TitleBarLayout out;
  for (int i = 0; i < TitleBarMetrics::count; ++i) {
    out.visible[i] = titlebar_control_visible(list[static_cast<std::size_t>(i)].action, c);
    if (out.visible[i]) ++out.count;
  }
  double x = window_w - static_cast<double>(out.count) * TitleBarMetrics::button;
  for (int i = 0; i < TitleBarMetrics::count; ++i) {
    if (!out.visible[i]) continue;
    out.rect[i] = TitleBarRect{x, 0.0, TitleBarMetrics::button, TitleBarMetrics::bar_height};
    x += TitleBarMetrics::button;
  }
  return out;
}

inline TitleBarRect titlebar_icon_rect(const TitleBarRect& button, bool hovered) {
  const double s = hovered ? TitleBarMetrics::icon_scale_hover : TitleBarMetrics::icon_scale;
  const double size = TitleBarMetrics::button * s;
  const double inset = (TitleBarMetrics::button - size) * 0.5;
  return TitleBarRect{button.x + inset, button.y + inset, size, size};
}

inline TitleBarRect titlebar_popup_rect(const TitleBarRect& button) {
  return TitleBarRect{button.x + button.w * 0.5 - TitleBarMetrics::popup_w * 0.5,
                      button.y + button.h, TitleBarMetrics::popup_w, TitleBarMetrics::popup_h};
}

inline void titlebar_popup_rects(const TitleBarRect& button, TitleBarRect& popup, TitleBarRect& track) {
  popup = titlebar_popup_rect(button);
  track = TitleBarRect{popup.x + TitleBarMetrics::popup_pad_x,
                       popup.y + TitleBarMetrics::popup_outer_pad + TitleBarMetrics::popup_pad_top,
                       TitleBarMetrics::slider_w,
                       TitleBarMetrics::slider_h};
}

inline double titlebar_slider_value(const TitleBarRect& track, double y) {
  if (!(track.h > 0.0)) return 0.0;
  const double v = 1.0 - (y - track.y) / track.h;
  return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

inline TitleBarRect titlebar_knob_rect(const TitleBarRect& track, double v) {
  const double bottom = v * (track.h - TitleBarMetrics::knob) + TitleBarMetrics::knob * 0.5;
  return TitleBarRect{track.x + track.w * 0.5, track.y + track.h - bottom, TitleBarMetrics::knob,
                      TitleBarMetrics::knob};
}

inline TitleBarRect titlebar_fill_rect(const TitleBarRect& track, double v) {
  return TitleBarRect{track.x, track.y + track.h * (1.0 - v), track.w, track.h * v};
}

inline bool titlebar_rect_contains(const TitleBarRect& r, double x, double y) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

enum class TitleHitKind { none, drag, button };

struct TitleHit {
  TitleHitKind kind = TitleHitKind::none;
  int index = -1;
};

inline TitleHit titlebar_hit(double x, double y, const TitleBarLayout& layout) {
  for (int i = 0; i < TitleBarMetrics::count; ++i) {
    if (!layout.visible[i]) continue;
    if (titlebar_rect_contains(layout.rect[i], x, y)) return TitleHit{TitleHitKind::button, i};
  }
  if (y >= 0.0 && y < TitleBarMetrics::drag_height) return TitleHit{TitleHitKind::drag, -1};
  return TitleHit{};
}

inline bool titlebar_swallows(double y) { return y >= 0.0 && y < TitleBarMetrics::drag_height; }

inline TitleEdge titlebar_edge_at(double wx, double wy, double win_w, double win_h,
                                 double band = TitleBarMetrics::resize_band) {
  const bool left = wx < band;
  const bool right = wx >= win_w - band;
  const bool top = wy < band;
  const bool bottom = wy >= win_h - band;
  if (left && top) return TitleEdge::top_left;
  if (right && top) return TitleEdge::top_right;
  if (left && bottom) return TitleEdge::bottom_left;
  if (right && bottom) return TitleEdge::bottom_right;
  if (top) return TitleEdge::top;
  if (bottom) return TitleEdge::bottom;
  if (left) return TitleEdge::left;
  if (right) return TitleEdge::right;
  return TitleEdge::none;
}

struct TitleBarState {
  int hover = -1;
  bool drag_hover = false;
  int popup = -1;
  int slider_drag = -1;
};

}
