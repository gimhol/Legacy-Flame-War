#include "lfw/cmds/cmd_pointer_events.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lfw/base/clock.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/cmd.h"
#include "lfw/defines/i_vector3.h"
#include "lfw/lfw.h"
#include "lfw/ui/ui_event.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/world.h"

namespace lfw {
namespace cmds {

namespace {

class UiSet {
 public:
  void add(ui::UINode* ui) {
    if (!has(ui)) _items.push_back(ui);
  }
  bool has(ui::UINode* ui) const {
    for (ui::UINode* item : _items) {
      if (item == ui) return true;
    }
    return false;
  }
  void del(ui::UINode* ui) {
    for (size_t i = 0; i < _items.size(); ++i) {
      if (_items[i] == ui) {
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(i));
        return;
      }
    }
  }
  void clear() { _items.clear(); }
  const std::vector<ui::UINode*>& items() const { return _items; }
  std::vector<ui::UINode*> live_items() {
    std::vector<ui::UINode*> out;
    out.reserve(_items.size());
    for (ui::UINode* const ui : _items) {
      if (is_live_uinode(ui)) out.push_back(ui);
    }
    _items = out;
    return out;
  }

 private:
  std::vector<ui::UINode*> _items;
};

struct IPointerState {
  UiSet down_uis;
  UiSet on_uis;
  std::unordered_map<std::u16string, IPointerCursor> cursors;
};

struct IPointerTargets {
  std::vector<std::vector<double>> paths;
  std::vector<std::vector<double>> points;
};

std::unordered_map<const World*, IPointerState>& pointer_states() {
  static std::unordered_map<const World*, IPointerState> states;
  return states;
}

IPointerState& state_of(World& world) { return pointer_states()[&world]; }

IPointerState& pointer_state(CMDS& c) { return state_of(c.world()); }

IPointerCursor& cursor_for(IPointerState& state, const std::u16string& from) {
  IPointerCursor& cursor = state.cursors[from];
  return cursor;
}

std::vector<std::u16string> split_char(const std::u16string& s, char16_t sep) {
  std::vector<std::u16string> out;
  std::u16string cur;
  for (char16_t c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

std::vector<double> nums_of(const std::u16string& raw) {
  std::vector<double> out;
  for (const std::u16string& part : split_char(raw, u',')) out.push_back(string_to_number(part));
  return out;
}

bool is_js_integer(double v) { return std::isfinite(v) && std::floor(v) == v; }

bool all_finite(const std::vector<double>& nums) {
  for (double v : nums) {
    if (!std::isfinite(v)) return false;
  }
  return true;
}

void warn_of(CMDS& c, const std::u16string& text) { c.world().lfw().warn(text); }

std::u16string count_of(size_t n) { return to_string(Value(static_cast<double>(n))); }

IPointerCursor* move_cursor(CMDS& c, std::optional<bool> hidden,
                            std::optional<bool> down = std::nullopt) {
  const std::optional<std::u16string> from = c.str_arg(u"--from");
  if (!from || from->empty()) return nullptr;
  IPointerCursor& cursor = cursor_for(pointer_state(c), *from);
  const std::optional<std::vector<double>> pos = c.nums_arg(u"--pos");
  if (pos && pos->size() > 1 && std::isfinite((*pos)[0]) && std::isfinite((*pos)[1])) {
    cursor.x = (*pos)[0];
    cursor.y = (*pos)[1];
  }
  if (hidden.has_value()) cursor.hidden = *hidden;
  if (down.has_value()) cursor.down = *down;
  cursor.t = clock_now();
  return &cursor;
}

std::optional<IPointerTargets> read_targets(const std::u16string& tag, CMDS& c) {
  const std::u16string paths_raw = c.str_arg(u"--paths").value_or(std::u16string());
  const std::u16string points_raw = c.str_arg(u"--points").value_or(std::u16string());
  const std::vector<std::u16string> path_parts =
      paths_raw.empty() ? std::vector<std::u16string>() : split_char(paths_raw, u'|');
  const std::vector<std::u16string> point_parts =
      points_raw.empty() ? std::vector<std::u16string>() : split_char(points_raw, u'|');
  if (!point_parts.empty() && point_parts.size() != path_parts.size()) {
    // `--points` 是「每个命中目标各自的命中点」，必须和 `--paths` 一一对应；
    // 只给一个点没有意义（那是 `--pos`，指针位置，无命中时也有）
    warn_of(c, u"[" + tag + u"] failed, '--points' count mismatch: " +
                   count_of(point_parts.size()) + u" points vs " +
                   count_of(path_parts.size()) + u" paths, got: " + c.cmd());
    return std::nullopt;
  }
  IPointerTargets targets;
  for (size_t i = 0; i < path_parts.size(); ++i) {
    const std::vector<double> path = nums_of(path_parts[i]);
    const bool has_point = i < point_parts.size() && !point_parts[i].empty();
    const std::vector<double> point = has_point ? nums_of(point_parts[i]) : std::vector<double>();
    bool path_ok = path.size() >= 2;
    for (size_t k = 0; path_ok && k < path.size(); ++k) {
      if (!is_js_integer(path[k]) || path[k] < 0) path_ok = false;
    }
    if (!path_ok) {
      warn_of(c, u"[" + tag + u"] failed, invalid path: '" + path_parts[i] + u"', got: " + c.cmd());
      return std::nullopt;
    }
    if (has_point && !all_finite(point)) {
      warn_of(c, u"[" + tag + u"] failed, invalid point: '" + point_parts[i] + u"', got: " +
                     c.cmd());
      return std::nullopt;
    }
    targets.paths.push_back(path);
    targets.points.push_back(point);
  }
  return targets;
}

std::optional<double> read_button(const std::u16string& tag, CMDS& c) {
  const std::optional<double> b = c.num_arg(u"--b");
  if (!b || !std::isfinite(*b)) {
    warn_of(c, u"[" + tag + u"] failed, invalid '--b', got: " + c.cmd());
    return std::nullopt;
  }
  return b;
}

ui::UINode* read_node(const std::u16string& tag, CMDS& c, const std::vector<double>& path) {
  const double layer_idx = path[0];
  const double page_idx = path[1];
  ui::UILayers* const layers = c.world().lfw().ui_layers();
  ui::UILayer* const layer = layers != nullptr ? layers->at(layer_idx) : nullptr;
  if (layer == nullptr) {
    warn_of(c, u"[" + tag + u"] layer not found: " + to_string(Value(layer_idx)));
    return nullptr;
  }
  ui::UINode* node = layer->at(page_idx);
  if (node == nullptr) {
    warn_of(c, u"[" + tag + u"] page not found: " + to_string(Value(page_idx)));
    return nullptr;
  }
  for (size_t i = 2; i < path.size(); ++i) {
    if (node == nullptr) break;
    const std::vector<ui::UINode*>& children = node->children();
    const size_t idx = static_cast<size_t>(path[i]);
    node = idx < children.size() ? children[idx] : nullptr;
  }
  if (node == nullptr) {
    std::u16string joined;
    for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0) joined += u",";
      joined += to_string(Value(path[i]));
    }
    warn_of(c, u"[" + tag + u"] node not found, path: " + joined);
  }
  return node;
}

std::vector<double> point_at(const IPointerTargets& targets, size_t i) {
  return i < targets.points.size() ? targets.points[i] : std::vector<double>();
}

ui::LFWPointerEvent make_event(const IPointerTargets& targets, size_t i, double b) {
  const std::vector<double> p = point_at(targets, i);
  const double x = p.size() > 0 ? p[0] : std::numeric_limits<double>::quiet_NaN();
  const double y = p.size() > 1 ? p[1] : std::numeric_limits<double>::quiet_NaN();
  const double z = p.size() > 2 ? p[2] : std::numeric_limits<double>::quiet_NaN();
  return ui::LFWPointerEvent(Vector3(x, y, z), b);
}

ui::LFWPointerEvent make_nan_event(double b) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  return ui::LFWPointerEvent(Vector3(nan, nan, nan), b);
}

}

const char16_t* cmd_pointer_down_help() {
  return uR"(Usage: POINTER_DOWN --paths=<layer>,<index>,<child>,... --points=<x,y,z> --b=<button>

Replay a pointer-down hit list on the local UI, near to far / 按近到远在本地 UI 上重放一次指针按下命中列表

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --points  每项命中点 x,y,z，与 --paths 一一对应（只给指针位置用 --pos）/ hit point of each entry, pairs with '--paths' (use '--pos' for the pointer position)
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --b       0 left / 1 middle / 2 right
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_move_help() {
  return uR"(Usage: POINTER_MOVE --paths=<layer>,<index>,<child>,... --points=<x,y,z> --b=<button>

Replay a pointer-move on the local UI, enter/leave are derived by diff; omit '--paths' for no hits / 在本地 UI 上重放一次指针移动，enter/leave 由差异推导；省略 '--paths' 表示无命中（全体 leave）

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --points  每项命中点 x,y,z，与 --paths 一一对应（只给指针位置用 --pos）/ hit point of each entry, pairs with '--paths' (use '--pos' for the pointer position)
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --b       0 left / 1 middle / 2 right
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_up_help() {
  return uR"(Usage: POINTER_UP --paths=<layer>,<index>,<child>,... --points=<x,y,z> --b=<button>

Replay a pointer-up on the local UI, then click and cancel leftovers; omit '--paths' for no hits / 重放指针抬起，随后派发 click 并取消未收到 up 的按下节点；省略 '--paths' 表示无命中（取消全部按下）

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --points  每项命中点 x,y,z，与 --paths 一一对应（只给指针位置用 --pos）/ hit point of each entry, pairs with '--paths' (use '--pos' for the pointer position)
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --b       0 left / 1 middle / 2 right
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_click_help() {
  return uR"(Usage: POINTER_CLICK --paths=<layer>,<index>,<child>,... --points=<x,y,z> --b=<button>

Replay one atomic click on the local UI, dispatch 'on_click' only, near to far / 在本地 UI 上重放一次原子点击，只派发 on_click（近到远）

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --points  每项命中点 x,y,z，与 --paths 一一对应（只给指针位置用 --pos）/ hit point of each entry, pairs with '--paths' (use '--pos' for the pointer position)
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --b       0 left / 1 middle / 2 right
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_cancel_help() {
  return uR"(Usage: POINTER_CANCEL --b=<button>

Cancel every node still pressed in the local UI / 取消本地 UI 中所有仍处于按下状态的节点

Options:
  --b       0 left / 1 middle / 2 right
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_enter_help() {
  return uR"(Usage: POINTER_ENTER --paths=<layer>,<index>,<child>,...

Dispatch pointer-enter on the local UI / 在本地 UI 上派发指针进入

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

const char16_t* cmd_pointer_leave_help() {
  return uR"(Usage: POINTER_LEAVE --paths=<layer>,<index>,<child>,...

Dispatch pointer-leave on the local UI; omit '--paths' when the pointer leaves the surface, leaving every hovered node and marking the initiator's cursor hidden / 在本地 UI 上派发指针离开；省略 '--paths' 表示指针离开画面：所有悬停节点 leave，并标记该发起方光标隐藏

Options:
  --paths   <layer>,<页面栈下标>,<子节点索引>...，多项以 | 分隔 / node paths, '|' separated
  --pos     指针位置 x,y（UI 坐标，无命中时也存在） / pointer position x,y (UI coordinates, present even without hits)
  --from    发起方 client id，联机时由网络层附加，本地省略 / initiator client id, attached by the net layer for netplay, omitted locally)";
}

std::unordered_map<std::u16string, IPointerCursor>* get_pointer_cursors(World& world) {
  std::unordered_map<const World*, IPointerState>& states = pointer_states();
  const std::unordered_map<const World*, IPointerState>::iterator it = states.find(&world);
  return it == states.end() ? nullptr : &it->second.cursors;
}

void set_local_cursor(World& world, double x, double y, std::optional<bool> hidden,
                      std::optional<bool> down) {
  IPointerCursor& cursor = cursor_for(state_of(world), u"");
  cursor.x = x;
  cursor.y = y;
  if (hidden.has_value()) cursor.hidden = *hidden;
  if (down.has_value()) cursor.down = *down;
  cursor.t = clock_now();
}

void cmd_pointer_down(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_DOWN;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  const std::optional<double> b = read_button(tag, c);
  if (!b) return;
  move_cursor(c, false, true);
  IPointerState& state = pointer_state(c);
  for (size_t i = 0; i < targets->paths.size(); ++i) {
    ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
    if (ui == nullptr || !ui->visible() || ui->disabled()) continue;
    state.down_uis.add(ui);
    ui::LFWPointerEvent event = make_event(*targets, i, *b);
    ui->on_pointer_down(event);
    if (event.stopped()) break;
  }
}

void cmd_pointer_move(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_MOVE;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  const std::optional<double> b = read_button(tag, c);
  if (!b) return;
  move_cursor(c, false);
  IPointerState& state = pointer_state(c);
  UiSet enter_uis;
  UiSet stay_uis;
  for (size_t i = 0; i < targets->paths.size(); ++i) {
    ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
    if (ui == nullptr || !ui->visible() || ui->disabled()) continue;
    ui::LFWPointerEvent event = make_event(*targets, i, *b);
    ui->on_pointer_move(event);
    if (state.on_uis.has(ui)) {
      state.on_uis.del(ui);
      stay_uis.add(ui);
    } else {
      enter_uis.add(ui);
    }
  }
  for (ui::UINode* ui : state.on_uis.live_items()) ui->on_pointer_leave();
  state.on_uis.clear();
  for (ui::UINode* ui : enter_uis.items()) {
    ui->on_pointer_enter();
    state.on_uis.add(ui);
  }
  for (ui::UINode* ui : stay_uis.items()) state.on_uis.add(ui);
}

void cmd_pointer_up(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_UP;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  const std::optional<double> b = read_button(tag, c);
  if (!b) return;
  move_cursor(c, false, false);
  IPointerState& state = pointer_state(c);
  {
    std::vector<ui::UINode*> uis;
    std::vector<size_t> pts;
    for (size_t i = 0; i < targets->paths.size(); ++i) {
      ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
      if (ui == nullptr || !ui->visible() || ui->disabled()) continue;
      uis.push_back(ui);
      pts.push_back(i);
    }
    const int button = static_cast<int>(*b);
    for (size_t i = 0; i < uis.size(); ++i) {
      ui::UINode* ui = uis[i];
      if (!ui->is_pointer_down(button)) continue;
      state.down_uis.del(ui);
      ui::LFWPointerEvent event = make_event(*targets, pts[i], *b);
      ui->on_pointer_up(event);
      if (event.stopped()) break;
    }
    for (size_t i = 0; i < uis.size(); ++i) {
      ui::UINode* ui = uis[i];
      if (ui->click_flag() == 0) continue;
      ui::LFWPointerEvent event = make_event(*targets, pts[i], *b);
      ui->on_click(event);
      if (event.stopped()) break;
    }
  }
  for (ui::UINode* ui : state.down_uis.live_items()) {
    ui::LFWPointerEvent event = make_nan_event(*b);
    ui->on_pointer_cancel(event);
  }
  state.down_uis.clear();
}

void cmd_pointer_click(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_CLICK;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  const std::optional<double> b = read_button(tag, c);
  if (!b) return;
  for (size_t i = 0; i < targets->paths.size(); ++i) {
    ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
    if (ui == nullptr || !ui->visible() || ui->disabled()) continue;
    ui::LFWPointerEvent event = make_event(*targets, i, *b);
    ui->on_click(event);
    if (event.stopped()) break;
  }
}

void cmd_pointer_cancel(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_CANCEL;
  const std::optional<double> b = read_button(tag, c);
  if (!b) return;
  move_cursor(c, std::nullopt, false);
  IPointerState& state = pointer_state(c);
  for (ui::UINode* ui : state.down_uis.live_items()) {
    ui::LFWPointerEvent event = make_nan_event(*b);
    ui->on_pointer_cancel(event);
  }
  state.down_uis.clear();
}

void cmd_pointer_enter(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_ENTER;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  move_cursor(c, false);
  IPointerState& state = pointer_state(c);
  for (size_t i = 0; i < targets->paths.size(); ++i) {
    ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
    if (ui == nullptr || !ui->visible() || ui->disabled()) continue;
    ui->on_pointer_enter();
    state.on_uis.add(ui);
  }
}

void cmd_pointer_leave(CMDS& c) {
  const std::u16string tag = cmd::kPOINTER_LEAVE;
  const std::optional<IPointerTargets> targets = read_targets(tag, c);
  if (!targets) return;
  IPointerState& state = pointer_state(c);
  if (targets->paths.empty()) {
    move_cursor(c, true);
    for (ui::UINode* ui : state.on_uis.live_items()) ui->on_pointer_leave();
    state.on_uis.clear();
    return;
  }
  for (size_t i = 0; i < targets->paths.size(); ++i) {
    ui::UINode* const ui = read_node(tag, c, targets->paths[i]);
    if (ui == nullptr) continue;
    ui->on_pointer_leave();
    state.on_uis.del(ui);
  }
}

}
}
