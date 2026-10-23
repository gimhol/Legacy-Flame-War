#include "host/godot/lfw_godot_ui_run.h"

#include <utility>

#include <godot_cpp/classes/rendering_server.hpp>

#include "host/godot/lfw_godot_node.h"
#include "lfw/base/perf.h"

namespace lfw::host {

void LfwGodotUiRun::assign(std::vector<GodotDrawItem> items, bool clip, const godot::Rect2& rect) {
  _items = std::move(items);
  _clipped = clip;
  _rect = rect;
  show();
  queue_redraw();
}

void LfwGodotUiRun::append(std::vector<GodotDrawItem> items) {
  if (items.empty()) return;
  _items.insert(_items.end(), items.begin(), items.end());
  show();
  queue_redraw();
}

void LfwGodotUiRun::clear() {
  if (_items.empty()) {
    hide();
    return;
  }
  _items.clear();
  queue_redraw();
  hide();
}

void LfwGodotUiRun::apply_clip() {
  godot::RenderingServer* const rs = godot::RenderingServer::get_singleton();
  if (rs == nullptr) return;
  const godot::RID rid = get_canvas_item();
  rs->canvas_item_set_clip(rid, _clipped);
  rs->canvas_item_set_custom_rect(rid, _clipped, _rect);
}

void LfwGodotUiRun::_draw() {
  if (_owner == nullptr) return;
  LFW_PERF_SCOPE(kPerfDraw);
  // 裁剪状态在 `_draw` 里设：和宿主裁游戏区（`apply_game_clip`）用的是同一条路径，
  // 只有在这里设才会作用于本 item 自己的绘制命令。
  apply_clip();
  const godot::Transform2D xform = _owner->item_xform();
  for (const GodotDrawItem& item : _items) {
    if (item.kind == kConsoleKind) {
      _owner->draw_console_into(*this);
      continue;
    }
    if (item.kind == kTitlebarKind) {
      _owner->draw_titlebar_into(*this);
      continue;
    }
    draw_set_transform_matrix(xform);
    if (!item.text.empty()) {
      _owner->draw_text_item(*this, item);
    } else {
      _owner->draw_item(*this, item);
    }
  }
  draw_set_transform_matrix(godot::Transform2D());
}

}
