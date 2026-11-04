#include "host/common/ui_input_handle.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lfw/camera.h"
#include "lfw/cmds/cmd_pointer_events.h"
#include "lfw/core/js_num.h"
#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/defines/cmd.h"
#include "lfw/ui/uilayer.h"
#include "lfw/world.h"

namespace lfw::host {

namespace {

double dataset_num(const World& world, const char16_t* key, double fallback) {
  const Value v = world.dataset.get(key);
  if (std::holds_alternative<std::monostate>(v)) return fallback;
  const double n = to_number(v);
  return std::isnan(n) ? fallback : n;
}

}

UiScreenSize ui_screen_size(const World& world) {
  UiScreenSize out;
  out.w = dataset_num(world, u"screen_w", 794.0);
  out.h = dataset_num(world, u"screen_h", 450.0);
  return out;
}

UiPoint ui_point_of_scene(const World& world, double scene_x, double scene_y) {
  const UiScreenSize s = ui_screen_size(world);
  return UiPoint{(scene_x + 1) / 2 * s.w, (1 - scene_y) / 2 * s.h};
}

void ui_scene_of_point(const World& world, double x, double y, double& scene_x, double& scene_y) {
  const UiScreenSize s = ui_screen_size(world);
  scene_x = s.w > 0 ? x / s.w * 2 - 1 : 0;
  scene_y = s.h > 0 ? -(y / s.h) * 2 + 1 : 0;
}

std::u16string ui_node_path(const ui::UINode& node) {
  ui::UILayer* const layer = node.layer();
  if (layer == nullptr) return {};
  const Value page_id = node.root().id();
  const std::u16string* const page_id_s = std::get_if<std::u16string>(&page_id);
  if (page_id_s == nullptr || page_id_s->empty()) return {};

  const ui::UINode* const page = &node.root();
  const std::vector<ui::UINode*> pages = layer->pages();
  double page_idx = -1;
  for (size_t i = 0; i < pages.size(); ++i) {
    if (pages[i] == page) {
      page_idx = static_cast<double>(i);
      break;
    }
  }
  if (page_idx < 0) return {};

  std::vector<double> idx;
  const ui::UINode* n = &node;
  while (n != nullptr && n != page) {
    ui::UINode* const parent = n->parent();
    if (parent == nullptr) return {};
    const std::vector<ui::UINode*>& children = parent->children();
    double found = -1;
    for (size_t i = 0; i < children.size(); ++i) {
      if (children[i] == n) {
        found = static_cast<double>(i);
        break;
      }
    }
    idx.insert(idx.begin(), found);
    n = parent;
  }

  std::u16string out = number_to_string(layer->index());
  out += u",";
  out += number_to_string(page_idx);
  for (const double v : idx) {
    out += u",";
    out += number_to_string(v);
  }
  return out;
}

void UiInputHandle::set_ui_offset(double x, double y, double z) {
  _offset_x = x;
  _offset_y = y;
  _offset_z = z;
}

void UiInputHandle::on_pointer_down(LfwPointingEvent& e) {
  update_local_cursor(e, false, true);
  push_pointer(cmd::kPOINTER_DOWN, e);
}

void UiInputHandle::on_pointer_move(LfwPointingEvent& e) {
  update_local_cursor(e, false);
  push_pointer(cmd::kPOINTER_MOVE, e);
}

void UiInputHandle::on_pointer_up(LfwPointingEvent& e) {
  update_local_cursor(e, false, false);
  push_pointer(cmd::kPOINTER_UP, e);
}

void UiInputHandle::on_pointer_cancel(LfwPointingEvent& e) {
  update_local_cursor(e, std::nullopt, false);
  _lfw->push_cmd({cmd::kPOINTER_CANCEL, u"--b=" + number_to_string(e.button), pos_arg(e)});
}

void UiInputHandle::on_pointer_enter(LfwPointingEvent& e) {
  update_local_cursor(e, false);
  push_pointer(cmd::kPOINTER_MOVE, e);
}

void UiInputHandle::on_pointer_leave(LfwPointingEvent& e) {
  if (_lfw->ui_layers() == nullptr || _lfw->ui_layers()->ui() == nullptr) return;
  update_local_cursor(e, true);
  _lfw->push_cmd({cmd::kPOINTER_LEAVE, pos_arg(e)});
}

std::u16string UiInputHandle::pos_arg(const LfwPointingEvent& e) const {  const UiScreenSize s = ui_screen_size(_lfw->world());
  const double x = js_round((e.scene_x + 1) / 2 * s.w * 10) / 10;
  const double y = js_round((1 - e.scene_y) / 2 * s.h * 10) / 10;
  return u"--pos=" + number_to_string(x) + u"," + number_to_string(y);
}

void UiInputHandle::update_local_cursor(const LfwPointingEvent& e, std::optional<bool> hidden,
                                        std::optional<bool> down) {
  const UiPoint p = ui_point_of_scene(_lfw->world(), e.scene_x, e.scene_y);
  cmds::set_local_cursor(_lfw->world(), p.x, p.y, hidden, down);
}

double UiInputHandle::world_z(const ui::UINode& node) const {
  double z = _offset_z;
  for (const ui::UINode* n = &node; n != nullptr; n = n->parent()) {
    const double pz = n->z();
    z += std::isfinite(pz) ? pz : 0.0;
  }
  return z;
}

std::vector<UiInputHandle::Hit> UiInputHandle::intersections(const LfwPointingEvent& e,
                                                             ui::UINode& page,
                                                             std::vector<std::u16string>* visits) const {
  const UiPoint p = ui_point_of_scene(_lfw->world(), e.scene_x, e.scene_y);
  const Camera& cam = _lfw->world().camera();
  const UiScreenSize s = ui_screen_size(_lfw->world());

  std::vector<Hit> hits;
  double order = 0;
  const std::function<void(ui::UINode&)> walk = [&](ui::UINode& node) {
    const double my_order = order++;
    if (visits != nullptr) {
      const ui::UINode::Geo& g = node.geo();
      visits->push_back(ui_node_path(node) + u"(" + number_to_string(g.left) + u"," +
                        number_to_string(g.top) + u"," + number_to_string(g.right) + u"," +
                        number_to_string(g.bottom) + u")v" + (node.visible() ? u"1" : u"0") + u"d" +
                        (node.disabled() ? u"1" : u"0"));
    }
    if (node.visible() && !node.disabled() && node.touchable()) {
      const ui::UINode::Geo& g = node.geo();
      if (p.x >= g.left && p.x <= g.right && p.y >= g.top && p.y <= g.bottom) {
        bool inside = true;
        for (ui::UINode* parent = node.parent(); parent != nullptr; parent = parent->parent()) {
          if (!parent->clip_children()) continue;
          const ui::UINode::Geo& pg = parent->geo();
          if (p.x < pg.left || p.x > pg.right || p.y < pg.top || p.y > pg.bottom) {
            inside = false;
            break;
          }
        }
        if (inside) {
          Hit hit;
          hit.node = &node;
          hit.x = cam.position.x + _offset_x + p.x;
          hit.y = cam.position.y + s.h + _offset_y - p.y;
          hit.z = world_z(node);
          hit.order = my_order;
          hits.push_back(hit);
        }
      }
    }
    for (ui::UINode* const child : node.children()) {
      if (child != nullptr) walk(*child);
    }
  };
  walk(page);

  std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
    return a.z != b.z ? a.z > b.z : a.order < b.order;
  });
  return hits;
}

void UiInputHandle::push_pointer(const char16_t* command, const LfwPointingEvent& e) {
  ui::UILayers* const layers = _lfw->ui_layers();
  if (layers == nullptr) return;

  const std::u16string_view name(command);
  // 指针只作用于最上面那个「有命中」的层：弹窗/浮层盖住下面的页面
  ui::UINode* page = nullptr;
  std::vector<Hit> hits;
  for (ui::UINode* const candidate : layers->layer_tops()) {
    if (candidate == nullptr) continue;
    std::vector<Hit> found = intersections(e, *candidate);
    if (found.empty()) continue;
    page = candidate;
    hits = std::move(found);
    break;
  }
  if (page == nullptr) return;

  std::vector<std::u16string> visits;
  if (_lfw->dev_mode && name == cmd::kPOINTER_DOWN) {
    static_cast<void>(intersections(e, *page, &visits));
  }

  std::u16string paths;
  std::u16string lpaths;
  std::u16string points;
  std::u16string lpoints;
  for (const Hit& hit : hits) {
    const std::u16string path = ui_node_path(*hit.node);
    if (path.empty()) continue;
    const std::u16string point = number_to_string(hit.x) + u"," + number_to_string(hit.y) + u"," +
                                 number_to_string(hit.z);
    if (hit.node->local_only()) {
      if (!lpaths.empty()) {
        lpaths += u"|";
        lpoints += u"|";
      }
      lpaths += path;
      lpoints += point;
    } else {
      if (!paths.empty()) {
        paths += u"|";
        points += u"|";
      }
      paths += path;
      points += point;
    }
  }

  const bool empty = paths.empty();
  if (empty && name == cmd::kPOINTER_DOWN && lpaths.empty()) return;

  if (_lfw->dev_mode && name == cmd::kPOINTER_DOWN) {
    std::u16string trace = u"[" + std::u16string(name) + u"] " + pos_arg(e) + u" hits=" +
                           number_to_string(static_cast<double>(hits.size()));
    for (const Hit& hit : hits) {
      const ui::UINode::Geo& g = hit.node->geo();
      trace += u" | " + ui_node_path(*hit.node) + u"(" + number_to_string(g.left) + u"," +
               number_to_string(g.top) + u"," + number_to_string(g.right) + u"," +
               number_to_string(g.bottom) + u")";
    }
    _lfw->host().log({Value(trace)});
    std::u16string tree = u"[" + std::u16string(name) + u"] tree:";
    for (const std::u16string& v : visits) tree += u" " + v;
    _lfw->host().log({Value(tree)});
  }

  if (!empty) {
    _lfw->push_cmd({std::u16string(name), u"--b=" + number_to_string(e.button), pos_arg(e),
                    u"--paths=" + paths, u"--points=" + points});
  }
  if (!lpaths.empty()) {
    _lfw->push_lcmd({std::u16string(name), u"--b=" + number_to_string(e.button), pos_arg(e),
                     u"--paths=" + lpaths, u"--points=" + lpoints});
  }
}

}
