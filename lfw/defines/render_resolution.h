#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/defines/defines_data.h"
#include "lfw/utils/string_help.h"

namespace lfw {
namespace defines {

/** `Defines.viewport_from_aspect` 的返回值。 */
struct ViewportSize {
  double w = 0.0;
  double h = 0.0;
};

/** 渲染分辨率预设（`Defines.RENDER_RESOLUTION_PRESETS`）。w/h 都为 0 表示跟随画布。 */
struct RenderResolutionPreset {
  double w = 0.0;
  double h = 0.0;
  std::u16string label;
  std::u16string aspect;
};

inline double safe_screen_w() { return defines::num(u"Defines.SAFE_SCREEN_WIDTH"); }

inline double safe_screen_h() { return defines::num(u"Defines.SAFE_SCREEN_HEIGHT"); }

/** 按目标宽高比生成逻辑视口：只向外扩，保证安全区完整可见。 */
inline ViewportSize viewport_from_aspect(double aspect) {
  if (!(aspect > 0.0)) return ViewportSize{safe_screen_w(), safe_screen_h()};
  return ViewportSize{std::max(safe_screen_w(), std::round(safe_screen_h() * aspect)),
                      std::max(safe_screen_h(), std::round(safe_screen_w() / aspect))};
}

namespace detail {

inline std::optional<double> aspect_number(const std::u16string& text) {
  const std::u16string t = js_trim(text);
  if (t.empty()) return std::nullopt;
  const double n = string_to_number(t);
  if (std::isnan(n) || !std::isfinite(n)) return std::nullopt;
  return n;
}

inline RenderResolutionPreset resolution(double w, double h, const char16_t* aspect) {
  RenderResolutionPreset ret;
  ret.w = w;
  ret.h = h;
  ret.aspect = aspect;
  ret.label = number_to_string(w) + u"×" + number_to_string(h) + u" (" + aspect + u")";
  return ret;
}

}  // namespace detail

/** 解析 `16:9` / `21:9` / `1.7778`；无效返回 `nullopt`（`Defines.parse_aspect`）。 */
inline std::optional<double> parse_aspect(const std::u16string& value) {
  const std::u16string text = js_trim(value);
  const size_t at = text.find_first_of(u":/");
  double aspect = 0.0;
  bool parsed = false;
  if (at != std::u16string::npos) {
    const std::optional<double> a = detail::aspect_number(text.substr(0, at));
    const std::optional<double> b = detail::aspect_number(text.substr(at + 1));
    if (a.has_value() && b.has_value() && *a > 0.0 && *b > 0.0) {
      aspect = *a / *b;
      parsed = true;
    }
  }
  if (!parsed) {
    const std::optional<double> n = detail::aspect_number(text);
    if (!n.has_value()) return std::nullopt;
    aspect = *n;
  }
  if (!std::isfinite(aspect) || aspect < 0.5 || aspect > 4.0) return std::nullopt;
  return aspect;
}

inline const std::vector<RenderResolutionPreset>& render_resolution_presets() {
  static const std::vector<RenderResolutionPreset> presets = [] {
    const double sw = safe_screen_w();
    const double sh = safe_screen_h();
    std::vector<RenderResolutionPreset> out;
    RenderResolutionPreset auto_preset;
    auto_preset.label = u"render_resolution_auto";
    out.push_back(auto_preset);
    out.push_back(detail::resolution(sw, sh, u"16:9"));
    out.push_back(detail::resolution(1280, 720, u"16:9"));
    out.push_back(detail::resolution(sw * 2, sh * 2, u"16:9"));
    out.push_back(detail::resolution(1920, 1080, u"16:9"));
    out.push_back(detail::resolution(sw * 3, sh * 3, u"16:9"));
    out.push_back(detail::resolution(sw * 4, sh * 4, u"16:9"));
    out.push_back(detail::resolution(1024, 768, u"4:3"));
    out.push_back(detail::resolution(1600, 1200, u"4:3"));
    out.push_back(detail::resolution(1280, 800, u"16:10"));
    out.push_back(detail::resolution(1920, 1200, u"16:10"));
    out.push_back(detail::resolution(2560, 1080, u"21:9"));
    out.push_back(detail::resolution(3440, 1440, u"21:9"));
    return out;
  }();
  return presets;
}

/** `Defines.DEFAULT_RENDER_RESOLUTION`（逻辑分辨率的 4 倍）。 */
inline const RenderResolutionPreset& default_render_resolution() {
  return render_resolution_presets()[6];
}

/** 当前 w/h 匹配的预设下标（不匹配返回 -1）。 */
inline int render_resolution_index(double w, double h) {
  const std::vector<RenderResolutionPreset>& presets = render_resolution_presets();
  for (size_t i = 0; i < presets.size(); ++i) {
    if (presets[i].w == w && presets[i].h == h) return static_cast<int>(i);
  }
  return -1;
}

/** 渲染分辨率 → 逻辑视口（w/h 非正时无结果）。 */
inline std::optional<ViewportSize> resolution_viewport(double w, double h) {
  if (!(w > 0.0) || !(h > 0.0)) return std::nullopt;
  return viewport_from_aspect(w / h);
}

}  // namespace defines
}  // namespace lfw
