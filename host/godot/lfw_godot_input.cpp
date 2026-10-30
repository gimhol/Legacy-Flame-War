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


std::u16string dom_key_of_godot(const godot::Ref<godot::InputEventKey>& key) {
  struct Entry {
    godot::Key code;
    const char16_t* name;
  };
  static const Entry kTable[] = {
      {godot::KEY_LEFT, u"arrowleft"},     {godot::KEY_RIGHT, u"arrowright"},
      {godot::KEY_UP, u"arrowup"},         {godot::KEY_DOWN, u"arrowdown"},
      {godot::KEY_ENTER, u"enter"},        {godot::KEY_KP_ENTER, u"enter"},
      {godot::KEY_ESCAPE, u"escape"},      {godot::KEY_TAB, u"tab"},
      {godot::KEY_BACKSPACE, u"backspace"},{godot::KEY_DELETE, u"delete"},
      {godot::KEY_INSERT, u"insert"},      {godot::KEY_HOME, u"home"},
      {godot::KEY_END, u"end"},            {godot::KEY_PAGEUP, u"pageup"},
      {godot::KEY_PAGEDOWN, u"pagedown"},  {godot::KEY_SHIFT, u"shift"},
      {godot::KEY_CTRL, u"control"},       {godot::KEY_ALT, u"alt"},
      {godot::KEY_META, u"meta"},          {godot::KEY_CAPSLOCK, u"capslock"},
      {godot::KEY_SPACE, u" "},            {godot::KEY_PERIOD, u"."},
      {godot::KEY_COMMA, u","},            {godot::KEY_SLASH, u"/"},
      {godot::KEY_SEMICOLON, u";"},        {godot::KEY_APOSTROPHE, u"'"},
      {godot::KEY_BRACKETLEFT, u"["},      {godot::KEY_BRACKETRIGHT, u"]"},
      {godot::KEY_BACKSLASH, u"\\"},       {godot::KEY_MINUS, u"-"},
      {godot::KEY_EQUAL, u"="},            {godot::KEY_QUOTELEFT, u"`"},
  };
  static const Entry kKeypad[] = {
      {godot::KEY_KP_0, u"0"}, {godot::KEY_KP_1, u"1"}, {godot::KEY_KP_2, u"2"},
      {godot::KEY_KP_3, u"3"}, {godot::KEY_KP_4, u"4"}, {godot::KEY_KP_5, u"5"},
      {godot::KEY_KP_6, u"6"}, {godot::KEY_KP_7, u"7"}, {godot::KEY_KP_8, u"8"},
      {godot::KEY_KP_9, u"9"}, {godot::KEY_KP_PERIOD, u"."}, {godot::KEY_KP_DIVIDE, u"/"},
      {godot::KEY_KP_MULTIPLY, u"*"}, {godot::KEY_KP_SUBTRACT, u"-"}, {godot::KEY_KP_ADD, u"+"},
  };
  const godot::Key code = key->get_keycode();
  for (const Entry& entry : kTable) {
    if (code == entry.code) return entry.name;
  }
  for (const Entry& entry : kKeypad) {
    if (code == entry.code) return entry.name;
  }
  if (code >= godot::KEY_F1 && code <= godot::KEY_F12) {
    return u"f" + std::u16string(1, static_cast<char16_t>(u'1' + (code - godot::KEY_F1)));
  }
  const char32_t unicode = key->get_unicode();
  if (unicode >= 32 && unicode <= 0x7e) {
    char16_t c = static_cast<char16_t>(unicode);
    if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
    return std::u16string(1, c);
  }
  if (code >= godot::KEY_A && code <= godot::KEY_Z) {
    return std::u16string(1, static_cast<char16_t>(code - godot::KEY_A + u'a'));
  }
  if (code >= godot::KEY_0 && code <= godot::KEY_9) {
    return std::u16string(1, static_cast<char16_t>(code));
  }
  return std::u16string();
}

}


void LfwGodot::report_key_focus(const std::u16string& key) {
  if (!_args.play || _lfw == nullptr) return;
  const ui::UINode* focused = nullptr;
  const auto walk = [&focused](const ui::UINode* n, const auto& self) -> void {
    if (n == nullptr || focused != nullptr) return;
    if (n->focused()) {
      focused = n;
      return;
    }
    for (ui::UINode* const c : n->children()) {
      self(c, self);
      if (focused != nullptr) return;
    }
  };
  for (ui::UINode* const layer : _lfw->layer_nodes()) {
    if (layer == nullptr) continue;
    walk(layer, walk);
    if (focused != nullptr) break;
  }
  std::string id = "(none)";
  bool has_click = false;
  if (focused != nullptr) {
    const Value& data = focused->data();
    if (const Value* const raw_id = ui::field_of(data, u"id")) {
      if (const std::u16string* const s = std::get_if<std::u16string>(raw_id)) id = to_utf8(*s);
    }
    const Value* const actions = ui::field_of(data, u"actions");
    if (actions != nullptr) {
      if (const Value* const click = ui::field_of(*actions, u"click")) has_click = truthy(*click);
    }
  }
  print_line("[godot] keyfocus: key=" + to_utf8(key) + " focused=" + id +
             " click=" + (has_click ? "yes" : "no") + " pages=" + ui_pages_text());
}

void LfwGodot::sync_text_input() {
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  if (renderer == nullptr) {
    _text_inputs.destroy();
    return;
  }
  const godot::Transform2D to_canvas =
      final_transform().affine_inverse() * contain_transform(window_size_px());
  _text_inputs.sync(*this, renderer->ui_text_inputs(), to_canvas,
                    [this](const std::string& css) { return font_for_css(css); });
}

void LfwGodot::probe_text_input(int frame) {
  if (!_args.text_input_given) return;
  const int at = _args.text_input_frame;
  GodotTextInputs::Overlay* const overlay = _text_inputs.last();
  if (frame == at) {
    if (overlay == nullptr) {
      print_error("[godot] text-input: no TextInput overlay on this page");
      return;
    }
    const bool had_focus = overlay->edit->has_focus();
    overlay->edit->grab_focus();
    overlay->edit->set_text(String());
    overlay->edit->set_caret_column(0);
    overlay->edit->insert_text_at_caret(String::utf8(to_utf8(_args.text_input).c_str()));
    print_line("[godot] text-input @frame " + std::to_string(frame) + " type='" +
               to_utf8(_args.text_input) + "' focus=" + (had_focus ? "1" : "0") +
               " overlays=" + std::to_string(_text_inputs.overlays().size()));
    return;
  }
  if (frame == at + 2) {
    if (overlay != nullptr) overlay->edit->release_focus();
    return;
  }
  if (frame != at + 4) return;
  std::string players;
  const godot::Rect2 rect =
      overlay != nullptr
          ? godot::Rect2(overlay->edit->get_global_position(), overlay->edit->get_size())
          : godot::Rect2();
  if (_lfw != nullptr) {
    for (const std::pair<std::u16string, std::unique_ptr<PlayerInfo>>& kv : _lfw->players()) {
      if (!players.empty()) players += ",";
      players += to_utf8(kv.first) + ":" +
                 (kv.second != nullptr ? to_utf8(lfw::to_string(kv.second->name()))
                                      : std::string("-"));
    }
  }
  print_line("[godot] text-input done: overlay=" +
             std::string(overlay != nullptr ? "1" : "0") + " rect=" +
             (rect.size.x <= 0.0f
                  ? std::string("-")
                  : (std::to_string(static_cast<int>(rect.position.x)) + "," +
                     std::to_string(static_cast<int>(rect.position.y)) + " " +
                     std::to_string(static_cast<int>(rect.size.x)) + "x" +
                     std::to_string(static_cast<int>(rect.size.y))))
                  + " shown='" +
             to_utf8(overlay != nullptr ? overlay->shown : std::u16string()) + "' comp='" +
             to_utf8(overlay != nullptr ? overlay->synced : std::u16string()) + "' players=" +
             (players.empty() ? std::string("-") : players));
}

void LfwGodot::push_window_click(double x, double y) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  for (int pass = 0; pass < 2; ++pass) {
    godot::Ref<godot::InputEventMouseButton> mb;
    mb.instantiate();
    mb->set_button_index(godot::MOUSE_BUTTON_LEFT);
    mb->set_pressed(pass == 0);
    mb->set_position(Vector2(static_cast<float>(x), static_cast<float>(y)));
    mb->set_global_position(mb->get_position());
    vp->push_input(mb, false);
  }
}

void LfwGodot::push_window_hover(double x, double y) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  godot::Ref<godot::InputEventMouseMotion> mm;
  mm.instantiate();
  mm->set_position(Vector2(static_cast<float>(x), static_cast<float>(y)));
  mm->set_global_position(mm->get_position());
  mm->set_relative(Vector2(0.0f, 0.0f));
  vp->push_input(mm, false);
}

void LfwGodot::push_window_drag(double x0, double y0, double x1, double y1) {
  godot::Viewport* const vp = get_viewport();
  if (vp == nullptr) return;
  godot::Ref<godot::InputEventMouseButton> down;
  down.instantiate();
  down->set_button_index(godot::MOUSE_BUTTON_LEFT);
  down->set_pressed(true);
  down->set_position(Vector2(static_cast<float>(x0), static_cast<float>(y0)));
  down->set_global_position(down->get_position());
  vp->push_input(down, false);
  constexpr int kSteps = 6;
  for (int i = 1; i <= kSteps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(kSteps);
    push_window_hover(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
  }
  godot::Ref<godot::InputEventMouseButton> up = down;
  up->set_pressed(false);
  up->set_position(Vector2(static_cast<float>(x1), static_cast<float>(y1)));
  up->set_global_position(up->get_position());
  vp->push_input(up, false);
}

LfwGodot::PointerKind LfwGodot::to_pointing_event(const godot::Ref<godot::InputEvent>& event,
                                                  LfwPointingEvent& out) const {
  double w = 794, h = 450;
  ui_screen_size(w, h);

  const godot::Ref<godot::InputEventPanGesture> pan = event;
  if (pan.is_valid()) {
    const godot::Vector2 d = pan->get_delta();
    out.delta_x = -static_cast<double>(d.x) * w;
    out.delta_y = -static_cast<double>(d.y) * h;
    out.button = 0;
    return PointerKind::wheel;
  }

  const godot::Ref<godot::InputEventMouseButton> mb = event;
  if (mb.is_valid()) {
    const godot::Vector2 pos = mb->get_position();
    out.x = static_cast<double>(pos.x);
    out.y = static_cast<double>(pos.y);
    out.delta_x = 0;
    out.delta_y = 0;
    const int64_t idx = static_cast<int64_t>(mb->get_button_index());
    if (idx == godot::MOUSE_BUTTON_WHEEL_UP || idx == godot::MOUSE_BUTTON_WHEEL_DOWN ||
        idx == godot::MOUSE_BUTTON_WHEEL_LEFT || idx == godot::MOUSE_BUTTON_WHEEL_RIGHT) {
      if (!mb->is_pressed()) return PointerKind::none;
      const double factor = std::fabs(static_cast<double>(mb->get_factor()));
      const double notch = 100.0 * (factor > 0.0 ? factor : 1.0);
      if (idx == godot::MOUSE_BUTTON_WHEEL_UP) {
        out.delta_y = -notch;
      } else if (idx == godot::MOUSE_BUTTON_WHEEL_DOWN) {
        out.delta_y = notch;
      } else if (idx == godot::MOUSE_BUTTON_WHEEL_LEFT) {
        out.delta_x = -notch;
      } else {
        out.delta_x = notch;
      }
      out.button = 0;
      return PointerKind::wheel;
    }
    out.button = idx == godot::MOUSE_BUTTON_LEFT     ? 0.0
                 : idx == godot::MOUSE_BUTTON_MIDDLE ? 1.0
                 : idx == godot::MOUSE_BUTTON_RIGHT  ? 2.0
                                                     : -1.0;
    return mb->is_pressed() ? PointerKind::down : PointerKind::up;
  }

  const godot::Ref<godot::InputEventMouseMotion> mm = event;
  if (mm.is_valid()) {
    const godot::Vector2 pos = mm->get_position();
    out.x = static_cast<double>(pos.x);
    out.y = static_cast<double>(pos.y);
    out.delta_x = 0;
    out.delta_y = 0;
    out.button = -1;
    return PointerKind::move;
  }
  return PointerKind::none;
}

void LfwGodot::_input(const godot::Ref<godot::InputEvent>& p_event) {
  if (!_running || _host == nullptr || _lfw == nullptr || p_event.is_null()) return;
  if (titlebar_consume(p_event)) {
    if (godot::Viewport* const vp = get_viewport()) vp->set_input_as_handled();
    return;
  }
  if (const godot::Ref<godot::InputEventKey> key_event = p_event; !key_event.is_null()) {
    if (_console.handle_key(key_event)) {
      if (godot::Viewport* const vp = get_viewport()) vp->set_input_as_handled();
      return;
    }
  }
  // 控制台开着：键盘 / 鼠标都不进游戏（LineEdit 自己在 GUI 阶段收字）
  if (_console.is_open()) {
    if (const godot::Ref<godot::InputEventMouseButton> mouse = p_event; !mouse.is_null()) {
      if (_console.handle_mouse(mouse)) {
        if (godot::Viewport* const vp = get_viewport()) vp->set_input_as_handled();
      }
    } else if (const godot::Ref<godot::InputEventMouseMotion> motion = p_event;
               !motion.is_null()) {
      _console.handle_motion(motion);
    }
    return;
  }
  if (const godot::Ref<godot::InputEventKey> key_event = p_event; !key_event.is_null()) {
    // Steam 自己没接管 Shift+Tab 时（Overlay 没注入 / 客户端设置关了）留一条手动路
    if (key_event->is_pressed() && key_event->is_shift_pressed() &&
        key_event->get_keycode() == godot::KEY_TAB && _steam.enabled()) {
      _steam.open_overlay("friends");
      print_line("[godot] steam: overlay -> friends (overlay_enabled=" +
                 std::to_string(_steam.overlay_enabled() ? 1 : 0) + ")");
      if (godot::Viewport* const vp = get_viewport()) vp->set_input_as_handled();
      return;
    }
    const std::u16string key = dom_key_of_godot(key_event);
    if (_args.play) {
      print_line("[godot] keyraw#" + std::to_string(++_key_seq) + ": " +
                 (key_event->is_pressed() ? "down" : "up") + " dom='" + to_utf8(key) +
                 "' keycode=" + std::to_string(static_cast<long long>(key_event->get_keycode())) +
                 " phys=" +
                 std::to_string(static_cast<long long>(key_event->get_physical_keycode())) +
                 " uni=" + std::to_string(static_cast<long long>(key_event->get_unicode())) +
                 " echo=" + (key_event->is_echo() ? "1" : "0"));
    }
    if (key.empty()) return;
    if (key_event->is_pressed()) {
      const bool first = _host->pressed_keys().find(key) == _host->pressed_keys().end();
      _host->key_down(key);
      if (first) {
        print_line("[godot] key: down " + to_utf8(key));
        report_key_focus(key);
      }
    } else {
      _host->key_up(key);
      print_line("[godot] key: up " + to_utf8(key));
    }
    return;
  }
  LfwPointingEvent e;
  const PointerKind kind = to_pointing_event(p_event, e);
  if (kind == PointerKind::none) return;
  if (kind == PointerKind::down) {
    const godot::Vector2 point(static_cast<float>(e.x), static_cast<float>(e.y));
    _text_inputs.release_focus_outside(point);
  }
  const godot::Vector2 port =
      (contain_transform(window_size_px()).affine_inverse() * final_transform())
          .xform(godot::Vector2(static_cast<float>(e.x), static_cast<float>(e.y)));
  e.x = static_cast<double>(port.x);
  e.y = static_cast<double>(port.y);
  ui_scene_of_point(_lfw->world(), e.x, e.y, e.scene_x, e.scene_y);
  switch (kind) {
    case PointerKind::down:
      _host->emit_pointer_down(e);
      break;
    case PointerKind::up:
      _host->emit_pointer_up(e);
      break;
    case PointerKind::move:
      _host->emit_pointer_move(e);
      break;
    case PointerKind::wheel:
      _host->emit_wheel(e);
      break;
    case PointerKind::none:
      break;
  }
}

LfwGodot::Args::ClickAt* LfwGodot::inject_clicks(int frame) {
  if (_host == nullptr || _lfw == nullptr) return nullptr;
  for (const Args::DragAt& drag : _args.drags) {
    if (drag.frame != frame) continue;
    push_window_drag(drag.x0, drag.y0, drag.x1, drag.y1);
    char buf[192];
    std::snprintf(buf, sizeof(buf), "[godot] drag @frame %d (%.1f, %.1f) -> (%.1f, %.1f)", frame,
                  drag.x0, drag.y0, drag.x1, drag.y1);
    print_line(buf);
  }
  if (_args.clicks.empty()) return nullptr;
  Args::ClickAt* first = nullptr;
  for (Args::ClickAt& click : _args.clicks) {
    if (click.frame != frame) continue;
    if (click.hover) {
      push_window_hover(click.x, click.y);
    } else if (click.window_space) {
      push_window_click(click.x, click.y);
    } else {
      _host->click_ui(*_lfw, click.x, click.y);
    }
    if (first == nullptr) first = &click;
  }
  return first;
}

const LfwGodot::Args::KeyAt* LfwGodot::inject_keys(int frame) {
  if (_args.keys.empty() || _host == nullptr || _lfw == nullptr) return nullptr;
  const Args::KeyAt* first = nullptr;
  for (const Args::KeyAt& key : _args.keys) {
    if (key.frame != frame) continue;
    if (key.down) {
      _host->key_down(key.key);
      report_key_focus(key.key);
    } else {
      _host->key_up(key.key);
    }
    if (first == nullptr) first = &key;
  }
  return first;
}

}
