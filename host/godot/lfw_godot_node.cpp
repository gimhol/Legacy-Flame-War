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


}


void LfwGodot::_process(double p_delta) {
  if (!_running) return;
  LFW_PERF_SCOPE(kPerfFrameTotal);
  LFW_PERF_BEGIN_PROCESS();
  // 窗口标题：引擎在 `_ready` 之后还会用 project name + " (DEBUG)" 覆盖一次，
  // 这里在第一帧改回来（`Window::set_title` 会同步到 DisplayServer）
  if (_frames == 0 && !_title_fixed) {
    _title_fixed = true;
    // Godot 给非导出运行的窗口标题追加 " (DEBUG)"（`window_set_title` 里也会加），
    // 所以最后用 Win32 直接改窗口文本
    if (godot::Window* const win = get_window()) win->set_title("Legacy Flame War");
    if (godot::DisplayServer* const ds = godot::DisplayServer::get_singleton()) {
      const int64_t handle = ds->window_get_native_handle(godot::DisplayServer::WINDOW_HANDLE, 0);
      if (handle != 0) {
        lfw::host::set_window_title(reinterpret_cast<void*>(static_cast<intptr_t>(handle)),
                                    "Legacy Flame War");
      }
    }
    apply_window_icon();
  }
  if (_console.is_open()) {
    const godot::Viewport* const vp = get_viewport();
    _console.sync(window_size_px(), vp != nullptr ? vp->get_final_transform()
                                                  : godot::Transform2D());
  }
  _audio.poll(_args.every > 0);
  _steam.poll();
  if (_steam_overlay_pending && _steam.overlay_enabled()) {
    _steam_overlay_pending = false;
    _steam.open_overlay("friends");
    print_line("[godot] steam: overlay -> friends");
  }
  if (_steam.enabled() && !_steam_overlay_warned && _frames >= 180 &&
      !_steam.overlay_enabled()) {
    _steam_overlay_warned = true;
    print_line(
        "[godot] steam: overlay 一直不可用（Steam 设置里的「游戏内界面」关了？渲染后端不是 "
        "Compatibility？）");
  }
  if (_capture_pending) {
    _capture_pending = false;
    capture_screenshot();
    finish();
    return;
  }
  const bool real_time =
      _args.frames <= 0 || godot::DisplayServer::get_singleton()->get_name() != "headless";
  double pacing = real_time ? p_delta : (_args.delta > 0.0 ? _args.delta : p_delta);
  if (!(pacing > 0.0)) pacing = 1.0 / 60.0;
  // UPS / playrate 在游戏里随时会变（设置页、`F5`），步长必须每帧跟着世界重算。用启动时的
  // 快照的话，30UPS 下每个 tick 前进 2 个 atom 却仍按 60 次/秒推 ⇒ 两倍速。
  _host->set_step_ms(_lfw->world().base_step_ms());
  _host->accumulate(pacing);

  const int shot_at = _args.shot_frame > 0 ? _args.shot_frame : -1;
  bool stepped = false;
  while (_running && (unlimited() || _frames < _args.frames) && _host->can_step()) {
    GodotWorldRenderer* const renderer = _host->world_renderer();
    const long long before = renderer != nullptr ? renderer->renders() : 0;
    const int frame = _frames;
    Args::ClickAt* const clicked = inject_clicks(frame);
    const Args::KeyAt* const keyed = inject_keys(frame);
    run_console_scripts(frame);
    {
      const double t_step = _stats_visible ? wall_now_ms() : 0.0;
      LFW_PERF_SCOPE(kPerfStep);
      _host->push_frame(_lfw->world());
      if (_stats_visible) {
        const double spent = wall_now_ms() - t_step;
        _step_cost_ms = _step_cost_ms > 0.0 ? _step_cost_ms * 0.9 + spent * 0.1 : spent;
      }
    }
    LFW_PERF_COUNT(kPerfSteps);
    {
      LFW_PERF_SCOPE(kPerfAssign);
      render_world_frame(before);
    }
    {
      LFW_PERF_SCOPE(kPerfSyncText);
      sync_text_input();
    }
    probe_text_input(frame);
    stepped = true;
    ++_frames;
    if (clicked != nullptr) {
      char buf[256];
      std::snprintf(buf, sizeof(buf), "[godot] %s @frame %d at=(%.1f, %.1f) pages=%s",
                    clicked->hover ? "hover-win" : (clicked->window_space ? "click-win" : "click"),
                    frame, clicked->x, clicked->y, ui_pages_text().c_str());
      print_line(buf);
    }
    if (keyed != nullptr) {
      const auto pressed = _host->pressed_keys().find(keyed->key);
      char buf[256];
      std::snprintf(buf, sizeof(buf), "[godot] key @frame %d %s %s times=%d pages=%s", frame,
                    keyed->down ? "down" : "up", to_utf8(keyed->key).c_str(),
                    pressed == _host->pressed_keys().end() ? 0 : pressed->second,
                    ui_pages_text().c_str());
      print_line(buf);
    }
    if (_args.every > 0 && (frame % _args.every == 0 || frame == _args.frames - 1)) {
      report_frame(frame);
    }
    if (shot_at >= 0 && _frames >= shot_at) {
      _capture_pending = true;
      break;
    }
  }
  if (stepped) queue_redraw();
  // 世界只在 `Sync` / `Half` 模式下自己按 tick 渲染；`FPS_60` / `FPS_120` / `Unlimited` 由帧回调
  // 按目标帧率补渲染（TS 那边是 rAF 回调，这里是 `_process`），这样 30UPS + 60FPS 能真的画 60 帧。
  if (render_fallback_due(pacing * 1000.0)) {
    GodotWorldRenderer* const renderer = _host->world_renderer();
    const long long before = renderer != nullptr ? renderer->renders() : 0;
    {
      LFW_PERF_SCOPE(kPerfRender);
      _lfw->world().render_once(pacing * 1000.0);
    }
    LFW_PERF_COUNT(kPerfRenders);
    {
      LFW_PERF_SCOPE(kPerfAssign);
      render_world_frame(before);
    }
    queue_redraw();
  }
#if LFW_PERF
  if (_frames - g_perf_window_start >= 600) {
    print_line(lfw::perf::summary(_frames - g_perf_window_start, _lfw->world().render_cost));
    g_perf_window_start = _frames;
    lfw::perf::reset();
  }
#endif
  if (!_ui_scroll_applied && !_args.ui_scroll.empty() && _frames >= 20) apply_ui_scroll();
  if (_capture_pending) return;
  if (!unlimited() && _frames >= _args.frames) finish();
}

void LfwGodot::_notification(int p_what) {
  if (p_what == NOTIFICATION_WM_SIZE_CHANGED) {
    queue_redraw();
    return;
  }
  if (p_what != NOTIFICATION_WM_CLOSE_REQUEST) return;
  _text_inputs.destroy();
  print_exit_summary();
}

void LfwGodot::_bind_methods() {}

}
