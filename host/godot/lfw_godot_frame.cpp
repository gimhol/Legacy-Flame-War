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

#if LFW_PERF
double g_tsc_per_ms = 1.0;

double steady_now_ms() {
  return static_cast<double>(__rdtsc()) / g_tsc_per_ms;
}

void calibrate_tsc() {
  using clock = std::chrono::steady_clock;
  const clock::time_point t0 = clock::now();
  const unsigned long long c0 = __rdtsc();
  double ms = 0.0;
  while (ms < 20.0) {
    ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
  }
  const unsigned long long c1 = __rdtsc();
  g_tsc_per_ms = static_cast<double>(c1 - c0) / ms;
}

long long g_perf_window_start = 0;
#endif

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


}


void LfwGodot::render_world_frame(long long renders_before) {
  GodotWorldRenderer* const renderer = _host->world_renderer();
  if (renderer == nullptr) return;
  if (renderer->renders() == renders_before) return;
  const GodotFrame& frame = renderer->take_frame();
  if (frame.valid) {
    _item_xform = final_transform().affine_inverse() * contain_transform(window_size_px());
    assign_ui_runs(frame);
  }
  if (_models != nullptr) {
    _model_textures.clear();
    for (const GodotDrawItem& item : frame.items) {
      if (!item.has_model) continue;
      godot::Ref<godot::ViewportTexture> tex = _models->sync(item.tag, item.model);
      if (!tex.is_null()) _model_textures[item.tag] = tex;
    }
    _models->end_frame();
  }
}

bool LfwGodot::render_fallback_due(double real_ms) {
  lfw::World& world = _lfw->world();
  const int mode = static_cast<int>(world.dataset.sync_render());
  // `Sync` 每个 tick 在 `update_once` 里渲染；`Half` 隔一个 tick 渲染，宿主不能再去补，
  // 否则补的那一帧正好把「减半」抵掉。
  if (mode == static_cast<int>(lfw::SyncRenderEnum::Sync)) return false;
  if (mode == static_cast<int>(lfw::SyncRenderEnum::Half)) return false;
  const double fps = world.fps_value();
  const double interval = fps > 0.0 ? 1000.0 / fps : 0.0;
  if (!(interval > 0.0)) return false;
  _render_wait_ms -= real_ms;
  if (_render_wait_ms > 0.0) return false;
  _render_wait_ms = interval;
  return true;
}

void LfwGodot::print_exit_summary() const {
  if (!_running || _lfw == nullptr || _host == nullptr) return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "[godot] exit: frames=%d clock=%.0fms steps=%lld pages=%s",
                _frames, _host->clock().now(), _host->steps(), ui_pages_text().c_str());
  print_line(buf);
  print_line("[godot] steam: " + _steam.status());
}

void LfwGodot::apply_ui_scroll() {
  _ui_scroll_applied = true;
  const std::string& spec = _args.ui_scroll;
  const size_t at = spec.find(':');
  if (at == std::string::npos) {
    print_error("[godot] --ui-scroll: want <node-id>:<offset>, got '" + spec + "'");
    return;
  }
  const std::string id_text = spec.substr(0, at);
  const double offset = std::atof(spec.c_str() + static_cast<std::ptrdiff_t>(at) + 1);
  const std::u16string id = from_utf8(id_text);

  ui::UILayers* const layers = _lfw != nullptr ? _lfw->ui_layers() : nullptr;
  ui::UINode* found = nullptr;
  if (layers != nullptr) {
    for (ui::UILayer* const layer : layers->all()) {
      for (ui::UINode* const page : layer->pages()) {
        if (page == nullptr) continue;
        found = page->search_node(id);
        if (found != nullptr) break;
      }
      if (found != nullptr) break;
    }
  }
  if (found == nullptr) {
    print_error("[godot] --ui-scroll: no such node id: " + id_text);
    return;
  }
  ui::UIComponent* const comp = found->find_component(ui::ScrollView::class_tag());
  if (comp == nullptr) {
    print_error("[godot] --ui-scroll: not a ScrollView: " + id_text);
    return;
  }
  static_cast<ui::ScrollView*>(comp)->scroll_to(offset);
  char buf[192];
  std::snprintf(buf, sizeof(buf), "[godot] ui scroll: %s -> %.1f", id_text.c_str(), offset);
  print_line(buf);
}

std::string LfwGodot::ui_pages_text() const {
  lfw::ui::UILayers* const layers = _lfw->ui_layers();
  if (layers == nullptr) return "<no layers>";
  std::string out;
  for (lfw::ui::UILayer* const layer : layers->all()) {
    if (layer == nullptr) continue;
    if (!out.empty()) out += " ";
    out += to_utf8(lfw::number_to_string(layer->index()));
    out += ":";
    lfw::ui::UINode* const page = layer->ui();
    out += page != nullptr ? to_utf8(lfw::to_string(page->id())) : "-";
  }
  return out.empty() ? "<empty>" : out;
}

void LfwGodot::_draw() {
  LFW_PERF_SCOPE(kPerfDraw);
  const double t_draw = _stats_visible ? wall_now_ms() : 0.0;
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  if (renderer == nullptr) return;
  const godot::Vector2i win = window_size_px();
  const godot::Transform2D contain = contain_transform(win);
  report_stretch(contain, contain_rect(win));
  apply_font_oversampling(contain);
  apply_game_clip(win);
  // 命令按 pass 排好序放在 `_draw_order` 里（`assign_ui_runs` 填）。带裁剪的那一段往后
  // 已经在各自的 canvas item 上画了（见 `LfwGodotUiRun`），这里只补前面这一段。
  draw_set_transform_matrix(_item_xform);
  const size_t head = std::min(_parent_draw_count, _draw_order.size());
  for (size_t i = 0; i < head; ++i) {
    const GodotDrawItem& item = *_draw_order[i];
    if (!item.text.empty()) {
      draw_text_item(*this, item);
    } else {
      draw_item(*this, item);
    }
  }
  draw_set_transform_matrix(godot::Transform2D());
  if (_args.clip_probe) draw_clip_probe(contain);
  // 没有裁剪段时标题栏 / 控制台还是自己画（和以前一样）
  if (head >= _draw_order.size()) {
    draw_titlebar_into(*this);
    draw_console_into(*this);
  }
  draw_stats_into(*this);
  if (_stats_visible) {
    const double spent = wall_now_ms() - t_draw;
    _draw_cost_ms = _draw_cost_ms > 0.0 ? _draw_cost_ms * 0.9 + spent * 0.1 : spent;
  }
  LFW_PERF_COUNT(kPerfDraws);
  LFW_PERF_ADD(kPerfDrawItems, static_cast<long long>(_draw_order.size()));
}

void LfwGodot::draw_console_into(godot::CanvasItem& canvas) {
  if (!_console.is_open()) return;
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), true);
  _console.draw(canvas, window_size_px());
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), false);
}

void LfwGodot::draw_stats_into(godot::CanvasItem& canvas) {
  if (!_stats_visible) return;
  const std::string text = stats_text();
  if (text.empty()) return;
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), true);
  // 和游戏内容用同一套变换（`_item_xform`）：Web 那边这块也在 canvas 里，跟着一起缩放；
  // 这样字体走的是视口 oversampling（缩放到 1.5x/2x 时仍然按放大后的尺寸栅格化，不会发虚）。
  canvas.draw_set_transform_matrix(_item_xform);
  godot::Ref<godot::Font> font = font_for_css("16px monospace");
  if (font.is_null()) font = _fonts.fallback();
  if (font.is_null()) {
    canvas.draw_set_transform_matrix(godot::Transform2D());
    return;
  }
  constexpr int kFontPx = 16;
  constexpr float kPad = 4.0f;
  std::vector<std::string> lines;
  double max_w = 0.0;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t nl = text.find('\n', start);
    lines.push_back(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
    max_w = std::max(max_w, static_cast<double>(
                                 font->get_string_size(godot::String::utf8(lines.back().c_str()),
                                                       godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f,
                                                       kFontPx)
                                     .x));
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  const float line_h = font->get_height(kFontPx);
  const double block_w = max_w + static_cast<double>(kPad) * 2.0;
  const double block_h = static_cast<double>(lines.size()) * static_cast<double>(line_h) +
                         static_cast<double>(kPad) * 2.0;
  canvas.draw_rect(godot::Rect2(0.0f, 0.0f, static_cast<float>(block_w), static_cast<float>(block_h)),
                   godot::Color(0.0f, 0.0f, 0.0f, 0.4f), true);
  double y = static_cast<double>(kPad) + static_cast<double>(font->get_ascent(kFontPx));
  for (const std::string& line : lines) {
    canvas.draw_string(font, godot::Vector2(kPad, static_cast<float>(y)),
                       godot::String::utf8(line.c_str()), godot::HORIZONTAL_ALIGNMENT_LEFT, -1.0f,
                       kFontPx, godot::Color(1.0f, 1.0f, 1.0f, 1.0f));
    y += static_cast<double>(line_h);
  }
  canvas.draw_set_transform_matrix(godot::Transform2D());
  if (rs != nullptr) rs->canvas_item_add_clip_ignore(canvas.get_canvas_item(), false);
}

std::string LfwGodot::stats_text() const {
  if (_lfw == nullptr) return std::string();
  const lfw::World& world = _lfw->world();
  const double fps = world.fps_value();
  const auto pad = [](const char* s, std::size_t w) {
    std::string out(s);
    if (out.size() < w) out.append(w - out.size(), ' ');
    return out;
  };
  char a[64];
  char b[64];
  char c[64];
  char d[64];
  std::string out;
  std::snprintf(a, sizeof(a), "FPS:%.0f", fps);
  std::snprintf(b, sizeof(b), "FRAME:%.2fms", fps > 0.0 ? 1000.0 / fps : 0.0);
  std::snprintf(c, sizeof(c), "UPS:%.0f x%.2f", world.dataset.UPS(), world.dataset.playrate());
  out += pad(a, 12) + pad(b, 14) + pad(c, 16) + "\n";
  std::snprintf(a, sizeof(a), "ENT:%d", static_cast<int>(world.entities.size()));
  std::snprintf(b, sizeof(b), "BUF:%d", static_cast<int>(world.buffs.size()));
  std::snprintf(c, sizeof(c), "COST:%.2fms", _step_cost_ms);
  std::snprintf(d, sizeof(d), "PAIR:%.0f", world.pairs_compared);
  out += pad(a, 12) + pad(b, 14) + pad(c, 16) + pad(d, 14) + "\n";
  std::snprintf(a, sizeof(a), "RND:%.2fms", _draw_cost_ms);
  std::snprintf(d, sizeof(d), "COLL:%d", static_cast<int>(world.collisions.size()));
  out += pad(a, 12) + pad(d, 14);
  return out;
}

void LfwGodot::set_stats_visible(bool visible) {
  if (_stats_visible == visible) return;
  _stats_visible = visible;
  queue_redraw();
}

void LfwGodot::assign_ui_runs(const GodotFrame& frame) {
  _draw_order.clear();
  _draw_order.reserve(frame.items.size());
  static const int kPasses[3] = {-1, 0, 1};
  for (const int pass : kPasses) {
    for (const GodotDrawItem& item : frame.items) {
      if (item.pass == pass) _draw_order.push_back(&item);
    }
  }

  // 第一条要裁剪的命令之前的部分由宿主自己的 canvas item 画（它只有游戏区那一个裁剪矩形）。
  // 从它开始，每换一个裁剪矩形就换一个 canvas item —— 这样才是按像素截断（TS 的 overflow:hidden）。
  size_t split = _draw_order.size();
  for (size_t i = 0; i < _draw_order.size(); ++i) {
    if (_draw_order[i]->has_clip) {
      split = i;
      break;
    }
  }
  _parent_draw_count = split;

  size_t used = 0;
  const auto same_clip = [](const GodotDrawItem& a, const GodotDrawItem& b) {
    return a.clip_x0 == b.clip_x0 && a.clip_y0 == b.clip_y0 && a.clip_x1 == b.clip_x1 &&
           a.clip_y1 == b.clip_y1;
  };
  const auto layer_for = [this](size_t index) {
    while (_ui_runs.size() <= index) {
      LfwGodotUiRun* const run = memnew(LfwGodotUiRun);
      char name[48];
      std::snprintf(name, sizeof(name), "lfw_ui_run_%d", static_cast<int>(_ui_runs.size() + 1));
      run->set_name(name);
      run->set_owner_node(this);
      run->set_z_index(static_cast<int32_t>(_ui_runs.size() + 1));
      run->hide();
      add_child(run);
      _ui_runs.push_back(run);
    }
    return _ui_runs[index];
  };

  size_t i = split;
  while (i < _draw_order.size()) {
    const GodotDrawItem& first = *_draw_order[i];
    std::vector<GodotDrawItem> slice;
    slice.push_back(first);
    ++i;
    while (i < _draw_order.size()) {
      const GodotDrawItem& next = *_draw_order[i];
      if (next.has_clip != first.has_clip) break;
      if (first.has_clip && !same_clip(first, next)) break;
      slice.push_back(next);
      ++i;
    }
    layer_for(used++)->assign(std::move(slice), first.has_clip,
                              godot::Rect2(static_cast<float>(first.clip_x0),
                                           static_cast<float>(first.clip_y0),
                                           static_cast<float>(first.clip_x1 - first.clip_x0),
                                           static_cast<float>(first.clip_y1 - first.clip_y0)));
  }
  for (size_t k = used; k < _ui_runs.size(); ++k) _ui_runs[k]->clear();

  // 标题栏 / 控制台要压在 UI 之上：并进最后一段（它们自己会 `clip_ignore`）
  if (used > 0) {
    std::vector<GodotDrawItem> tail;
    if (titlebar_visible()) {
      GodotDrawItem item;
      item.kind = LfwGodotUiRun::kTitlebarKind;
      item.w = 1.0;
      item.h = 1.0;
      tail.push_back(item);
    }
    if (_console.is_open()) {
      GodotDrawItem item;
      item.kind = LfwGodotUiRun::kConsoleKind;
      item.w = 1.0;
      item.h = 1.0;
      tail.push_back(item);
    }
    if (!tail.empty()) layer_for(used - 1)->append(std::move(tail));
  }
}

void LfwGodot::draw_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item) {
  host::draw_text_item(canvas, item, [this](const std::string& css) { return font_for_css(css); });
}

void LfwGodot::draw_item(godot::CanvasItem& canvas, const GodotDrawItem& item) {
  if (!(item.w > 0.0) || !(item.h > 0.0)) return;
  const float x = static_cast<float>(item.x);
  const float y = static_cast<float>(item.y);
  const float w = static_cast<float>(item.w);
  const float h = static_cast<float>(item.h);
  if (item.has_model) {
    const auto found = _model_textures.find(item.tag);
    if (found == _model_textures.end() || found->second.is_null()) return;
    LFW_PERF_COUNT(kPerfDrawOps);
    canvas.draw_texture_rect(found->second, Rect2(x, y, w, h), false);
    return;
  }
  const float alpha = static_cast<float>(item.a);
  const Color color(static_cast<float>(item.r), static_cast<float>(item.g),
                    static_cast<float>(item.b), alpha);
  if (item.flat) {
    LFW_PERF_COUNT(kPerfDrawOps);
    canvas.draw_rect(Rect2(x, y, w, h), color, true);
    return;
  }
  godot::Ref<godot::ImageTexture> tex;
  if (_textures != nullptr && !item.path.empty()) {
    LFW_PERF_COUNT(kPerfTexGets);
    tex = _textures->get(item.path);
  }
  if (tex.is_null()) {
    const std::uint32_t hash = item.tag_hash != 0 ? item.tag_hash : tag_hash_of(item.tag);
    const float hue = static_cast<float>(hash % 360u) / 360.0f;
    const Color tint = Color::from_hsv(hue, 0.55f, 0.85f, alpha);
    LFW_PERF_ADD(kPerfDrawOps, 2);
    canvas.draw_rect(Rect2(x, y, w, h), tint, true);
    canvas.draw_rect(Rect2(x, y, w, h), Color(0.1f, 0.1f, 0.15f, alpha), false, 1.0f);
    return;
  }
  if (item.mix_a > 0.0 || item.gray > 0.0) {
    LFW_PERF_COUNT(kPerfTintGets);
    const godot::Ref<godot::ImageTexture> baked = _textures->get_tinted(
        item.path, item.mix_r, item.mix_g, item.mix_b, item.gray, item.mix_a);
    if (baked.is_valid()) tex = baked;
  }
  const Rect2 src(static_cast<float>(item.sx * item.tex_scale),
                  static_cast<float>(item.sy * item.tex_scale),
                  static_cast<float>(item.sw * item.tex_scale),
                  static_cast<float>(item.sh * item.tex_scale));
  const bool rotated = item.rot != 0.0;
  godot::Transform2D spin_xform;
  if (rotated) {
    godot::Transform2D rot2;
    rot2.set_rotation(static_cast<godot::real_t>(item.rot));
    if (item.flip_x) {
      godot::Transform2D local_mirror;
      local_mirror.set_scale(Vector2(-1.0f, 1.0f));
      rot2 = rot2 * local_mirror;
    }
    godot::Transform2D to_center;
    to_center.set_origin(Vector2(x + w * 0.5f, y + h * 0.5f));
    spin_xform = _item_xform * to_center * rot2;
  }
  if (item.uv_tile && !rotated && !item.flip_x) {
    const double px = item.uv_px > 0.0 ? item.uv_px : item.w;
    const double py = item.uv_py > 0.0 ? item.uv_py : item.h;
    if (px > 0.0 && py > 0.0) {
      const double ox = -std::fmod(item.uv_ox, px);
      double oy = std::fmod(item.uv_oy, py);
      if (oy > 0.0) oy -= py;
      const auto tile_pass = [&](const Color& c) {
        for (double gy = oy; gy < item.h; gy += py) {
          for (double gx = ox; gx < item.w; gx += px) {
            LFW_PERF_COUNT(kPerfDrawOps);
            canvas.draw_texture_rect_region(
                tex, Rect2(static_cast<float>(item.x + gx), static_cast<float>(item.y + gy),
                           static_cast<float>(px), static_cast<float>(py)),
                src, c);
          }
        }
      };
      tile_pass(color);
      return;
    }
  }
  const auto blit = [&](const godot::Ref<godot::ImageTexture>& t, float ox, float oy, const Color& c) {
    LFW_PERF_COUNT(kPerfDrawOps);
    if (rotated) {
      godot::Transform2D push;
      push.set_origin(Vector2(ox, oy));
      canvas.draw_set_transform_matrix(spin_xform * push);
      canvas.draw_texture_rect_region(t, Rect2(-w * 0.5f, -h * 0.5f, w, h), src, c);
      canvas.draw_set_transform_matrix(_item_xform);
      return;
    }
    if (item.flip_x) {
      godot::Transform2D mirror;
      mirror.set_scale(Vector2(-1.0f, 1.0f));
      mirror.set_origin(Vector2(x + ox + w, y + oy));
      canvas.draw_set_transform_matrix(_item_xform * mirror);
      canvas.draw_texture_rect_region(t, Rect2(0.0f, 0.0f, w, h), src, c);
      canvas.draw_set_transform_matrix(_item_xform);
      return;
    }
    canvas.draw_texture_rect_region(t, Rect2(x + ox, y + oy, w, h), src, c);
  };
  if (item.outline_w > 0.0 && item.outline_a > 0.0 && !item.path.empty() && _textures != nullptr) {
    LFW_PERF_COUNT(kPerfSilGets);
    const godot::Ref<godot::ImageTexture> silhouette = _textures->get_silhouette(item.path);
    const godot::Ref<godot::ImageTexture> outline_tex =
        silhouette.is_valid() ? silhouette : tex;
    if (outline_tex.is_valid()) {
      const double r = item.outline_w;
      const Color oc(static_cast<float>(item.outline_r), static_cast<float>(item.outline_g),
                     static_cast<float>(item.outline_b), static_cast<float>(item.outline_a));
      blit(outline_tex, static_cast<float>(-r), 0.0f, oc);
      blit(outline_tex, static_cast<float>(r), 0.0f, oc);
      blit(outline_tex, 0.0f, static_cast<float>(-r), oc);
      blit(outline_tex, 0.0f, static_cast<float>(r), oc);
    }
  }
  blit(tex, 0.0f, 0.0f, color);
}

bool LfwGodot::capture_screenshot() {
  if (_args.screenshot.empty() || _captured) return false;
  _captured = true;
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs != nullptr) rs->force_draw();
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) {
    print_error("[godot] screenshot: no viewport");
    return false;
  }
  const godot::Ref<godot::ViewportTexture> vtex = vp->get_texture();
  if (vtex.is_null()) {
    print_error("[godot] screenshot: no viewport texture");
    return false;
  }
  const godot::Ref<godot::Image> img = vtex->get_image();
  if (img.is_null()) {
    print_error("[godot] screenshot: null image (headless has no renderer ⇒ drop --headless)");
    return false;
  }
  const godot::String path(_args.screenshot.generic_string().c_str());
  const godot::Error err = img->save_png(path);
  char buf[512];
  std::snprintf(buf, sizeof(buf), "[godot] screenshot: %s %dx%d err=%d frame=%d life=%.0f",
                _args.screenshot.generic_string().c_str(), img->get_width(), img->get_height(),
                static_cast<int>(err), _frames, _lfw != nullptr ? _lfw->world().lifetime() : 0.0);
  print_line(buf);
  return err == godot::OK;
}

void LfwGodot::run_console_scripts(int frame) {
  for (ConsoleSpec& it : _args.console_exec) {
    if (it.done) continue;
    const bool fire = frame < 0 ? it.frame <= 0 : it.frame == frame;
    if (!fire) continue;
    it.done = true;
    _console.execute(it.line);
  }
  for (ConsoleSpec& it : _args.console_text) {
    if (it.done) continue;
    const bool fire = frame < 0 ? it.frame <= 0 : it.frame == frame;
    if (!fire) continue;
    it.done = true;
    _console.set_input_text(it.line);
    print_line("[godot] console-text: '" + it.line + "' suggestions=" +
               (_console.suggestion_summary().empty() ? std::string("-")
                                                      : _console.suggestion_summary()));
  }
}

void LfwGodot::report_frame(int frame) const {
  const lfw::World& world = _lfw->world();
  const std::string digest = frame_digest(world, frame);
  char buf[384];
  std::snprintf(buf, sizeof(buf),
                "[godot frame %6d] entities=%zu ghosts=%zu hp=%.1f digest=%s pages=%s", frame,
                world.entities.size(), world.ghosts.size(), total_hp(world), digest.c_str(),
                ui_pages_text().c_str());
  print_line(buf);
}

void LfwGodot::finish() {
  _running = false;
  const lfw::World& world = _lfw->world();
  const std::string digest = frame_digest(world, _frames - 1);
  GodotWorldRenderer* const renderer = _host->world_renderer();
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "[godot] done: frames=%d ups=%.0f digest=%s warns=%d errors=%d ignored_images=%lld "
                "renderer(+-)=%lld/%lld clock=%.1fms residual=%.1fms steps=%lld",
                _frames, 1000.0 / _host->step_ms(), digest.c_str(), _host->warn_count(),
                _host->error_count(), _host->ignored_images(),
                renderer != nullptr ? renderer->added() : 0,
                renderer != nullptr ? renderer->removed() : 0, _host->clock().now(),
                _host->residual_ms(), _host->steps());
  print_line(buf);
  print_line("[godot] steam: " + _steam.status());
#if LFW_PERF
  print_line(lfw::perf::summary(_frames - g_perf_window_start, world.render_cost));
#endif
  std::snprintf(buf, sizeof(buf), "[godot] UI render calls: %lld (unhandled: %lld)",
                world.ui_render_calls(), world.ui_render_unhandled());
  print_line(buf);
  print_line("[godot] sounds: " + _audio.report());
  if (renderer != nullptr) {
    const GodotFrame& f = renderer->take_frame();
    std::snprintf(
        buf, sizeof(buf),
        "[godot] render: bg=%s cam=%.1f,%.1f scale=%.2f,%.2f df=%.3f items=%zu bg_layers=%d "
        "ents=%d hidden=%d no_pic=%d no_file=%d unnamed_tex=%d outline=%d[%s] rot=%d@%.4f "
        "model=%d[%s] bars=%d textures=%lld ok/%lld fail uv=%d[%s]",
        f.bg_id.c_str(), f.cam_x, f.cam_y, f.scale_x, f.scale_y, f.dfactor, f.items.size(),
        f.bg_layers, f.drawn_entities, f.hidden_entities, f.no_pic_entities, f.no_file_entities,
        f.unnamed_textures, f.outline_items, f.outline_probe.c_str(), f.rotated_items,
        f.rotated_probe, f.model_items, f.model_probe.c_str(), f.bar_items,
        _textures != nullptr ? _textures->loaded() : 0,
        _textures != nullptr ? _textures->failed() : 0, f.uv_items, f.uv_probe.c_str());
    print_line(buf);
    {
      const lfw::GenFieldStats& gen = lfw::gen_field_stats();
      std::snprintf(buf, sizeof(buf), "[godot] gen_field: calls=%lld resolved=%lld closures=%lld",
                    gen.calls, gen.resolved, gen.closures);
      print_line(buf);
    }
    if (_textures != nullptr && !_textures->last_error().empty()) {
      print_line("[godot] last texture error: " + _textures->last_error());
    }
    if (_models != nullptr && (!_models->last_error().empty() || _models->loaded() > 0)) {
      char mline[512];
      std::snprintf(mline, sizeof(mline),
                    "[godot] models: loaded=%lld failed=%lld live=%lld %s%s",
                    _models->loaded(), _models->failed(), _models->live(),
                    _models->report().c_str(), _models->last_error().c_str());
      print_line(mline);
    }
    const GodotUIStats& ui = renderer->ui_stats();
    std::snprintf(buf, sizeof(buf),
                  "[godot] ui: layers=%lld nodes=%lld drawn=%lld items=%lld texts=%lld hidden=%lld "
                  "empty=%lld clipped=%lld missing_img=%lld total_items=%zu",
                  ui.layers, ui.nodes, ui.drawn, ui.items, ui.texts, ui.hidden, ui.empty,
                  ui.clipped, ui.missing_img, f.items.size());
    print_line(buf);
  }
  if (_args.ui_dump && renderer != nullptr) {
    for (const std::string& line : renderer->ui_dump_lines()) print_line(line);
  }
  quit_with(0);
}

void LfwGodot::quit_with(int code) const {
  godot::SceneTree* const tree = get_tree();
  if (tree != nullptr) tree->quit(code);
}

}
