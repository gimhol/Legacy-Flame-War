#include "lfw/ui/flex_layout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include "lfw/ui/uinode.h"
#include "lfw/utils/math/round_float.h"

namespace lfw::ui {

namespace {

constexpr double kEpsilon = 1e-6;

bool is_row_direction(const std::u16string& direction) {
  return direction == u"row" || direction == u"row-reverse";
}

bool is_reverse_direction(const std::u16string& direction) {
  return direction == u"row-reverse" || direction == u"column-reverse";
}

std::u16string flip_justify(const std::u16string& v) {
  if (v == u"flex-start") return u"flex-end";
  if (v == u"flex-end") return u"flex-start";
  return v;
}

struct Line {
  std::vector<FlexItemInput> items;
  std::vector<double> base;
  std::vector<double> main;
  double cross = 0.0;
  double top = 0.0;
};

}  // namespace

std::vector<FlexItemOutput> compute_flex_layout(const FlexContainerInput& container,
                                                const std::vector<FlexItemInput>& items) {
  std::vector<FlexItemOutput> out;
  if (items.empty()) return out;

  const double content_w = std::max(0.0, container.width - container.padding.left -
                                               container.padding.right);
  const double content_h = std::max(0.0, container.height - container.padding.top -
                                               container.padding.bottom);

  const bool is_row = is_row_direction(container.direction);
  const bool reverse = is_reverse_direction(container.direction);
  const double main_size = is_row ? content_w : content_h;
  const double cross_size = is_row ? content_h : content_w;
  const double main_gap = std::max(0.0, is_row ? container.col_gap : container.row_gap);
  const double line_gap = std::max(0.0, is_row ? container.row_gap : container.col_gap);

  std::vector<FlexItemInput> ordered = items;
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const FlexItemInput& a, const FlexItemInput& b) { return a.order < b.order; });
  if (reverse) std::reverse(ordered.begin(), ordered.end());

  const auto base_of = [is_row](const FlexItemInput& item) -> double {
    if (item.basis_auto) return is_row ? item.width : item.height;
    return std::max(0.0, item.basis);
  };
  const auto cross_of = [is_row](const FlexItemInput& item) -> double {
    return is_row ? item.height : item.width;
  };

  std::vector<Line> lines;
  const auto push_line = [&lines, &base_of](const std::vector<FlexItemInput>& line_items) {
    Line line;
    line.items = line_items;
    for (const FlexItemInput& item : line_items) line.base.push_back(base_of(item));
    line.main = line.base;
    lines.push_back(std::move(line));
  };
  if (container.wrap == u"nowrap") {
    push_line(ordered);
  } else {
    std::vector<FlexItemInput> current;
    double used = 0.0;
    for (const FlexItemInput& item : ordered) {
      const double size = base_of(item);
      const double next = !current.empty() ? used + main_gap + size : size;
      if (!current.empty() && next > main_size) {
        push_line(current);
        current.clear();
        current.push_back(item);
        used = size;
      } else {
        current.push_back(item);
        used = next;
      }
    }
    if (!current.empty()) push_line(current);
  }

  for (Line& line : lines) {
    const size_t count = line.items.size();
    const double gaps = static_cast<double>(count > 0 ? count - 1 : 0) * main_gap;
    double total = gaps;
    for (double v : line.base) total += v;
    const double free = main_size - total;
    if (free > kEpsilon) {
      double grow_sum = 0.0;
      for (const FlexItemInput& item : line.items) grow_sum += item.grow;
      if (grow_sum > 0.0) {
        for (size_t i = 0; i < count; ++i) {
          line.main[i] += free * line.items[i].grow / grow_sum;
        }
      }
    } else if (free < -kEpsilon) {
      double shrink_sum = 0.0;
      for (size_t i = 0; i < count; ++i) shrink_sum += line.items[i].shrink * line.base[i];
      if (shrink_sum > 0.0) {
        for (size_t i = 0; i < count; ++i) {
          const double delta = -free * (line.items[i].shrink * line.base[i]) / shrink_sum;
          line.main[i] = std::max(0.0, line.base[i] - delta);
        }
      }
    }
    for (const FlexItemInput& item : line.items) line.cross = std::max(line.cross, cross_of(item));
  }

  double cross_total = 0.0;
  for (const Line& line : lines) cross_total += line.cross;
  double cross_free =
      cross_size -
      (cross_total + std::max(0.0, static_cast<double>(lines.size() > 0 ? lines.size() - 1 : 0)) *
                          line_gap);
  double line_cursor = 0.0;
  double line_extra = 0.0;
  if (container.align_content == u"stretch") {
    const double free = std::max(0.0, cross_free);
    const double total_cross = cross_total;
    for (Line& line : lines) {
      line.cross += total_cross > 0.0 ? free * line.cross / total_cross
                                      : free / static_cast<double>(lines.size());
    }
    cross_free = 0.0;
  } else if (cross_free > 0.0) {
    if (container.align_content == u"flex-end") {
      line_cursor = cross_free;
    } else if (container.align_content == u"center") {
      line_cursor = cross_free / 2.0;
    } else if (container.align_content == u"space-between") {
      line_extra = lines.size() > 1 ? cross_free / static_cast<double>(lines.size() - 1) : 0.0;
    } else if (container.align_content == u"space-around") {
      line_extra = cross_free / static_cast<double>(lines.size());
      line_cursor = line_extra / 2.0;
    }
  }
  for (Line& line : lines) {
    line.top = line_cursor;
    line_cursor += line.cross + line_gap + line_extra;
  }
  if (container.wrap == u"wrap-reverse") {
    for (Line& line : lines) line.top = cross_size - (line.top + line.cross);
  }

  const std::u16string justify = reverse ? flip_justify(container.justify) : container.justify;
  for (const Line& line : lines) {
    const size_t count = line.items.size();
    const double gaps = static_cast<double>(count > 0 ? count - 1 : 0) * main_gap;
    double sum = gaps;
    for (double v : line.main) sum += v;
    const double remain = std::max(0.0, main_size - sum);
    double main_cursor = 0.0;
    double main_extra = 0.0;
    if (justify == u"flex-end") {
      main_cursor = remain;
    } else if (justify == u"center") {
      main_cursor = remain / 2.0;
    } else if (justify == u"space-between") {
      main_extra = count > 1 ? remain / static_cast<double>(count - 1) : 0.0;
    } else if (justify == u"space-around") {
      main_extra = remain / static_cast<double>(count);
      main_cursor = main_extra / 2.0;
    } else if (justify == u"space-evenly") {
      main_extra = remain / static_cast<double>(count + 1);
      main_cursor = main_extra;
    }
    for (size_t i = 0; i < count; ++i) {
      const FlexItemInput& item = line.items[i];
      const std::u16string& self =
          item.align_self == u"auto" ? container.align : item.align_self;
      const bool stretch = self == u"stretch" && item.stretchable;
      const double cross_len = stretch ? line.cross : cross_of(item);
      const double cross_offset = self == u"flex-end"
                                      ? line.cross - cross_len
                                      : (self == u"center" ? (line.cross - cross_len) / 2.0 : 0.0);
      const double main_len = line.main[i];
      FlexItemOutput one;
      one.key = item.key;
      one.x = is_row ? main_cursor : line.top + cross_offset;
      one.y = is_row ? line.top + cross_offset : main_cursor;
      one.width = is_row ? main_len : cross_len;
      one.height = is_row ? cross_len : main_len;
      out.push_back(one);
      main_cursor += main_len + main_gap + main_extra;
    }
  }
  return out;
}

void apply_flex_layout(UINode& node) {
  std::vector<UINode*> targets;
  std::vector<FlexItemInput> items;
  const FlexStyle& flex = node.flex;
  const bool is_row = is_row_direction(flex.direction());
  for (UINode* const child : node.children()) {
    if (child == nullptr || !child->self_visible()) continue;
    const bool fits_cross = is_row ? child->flex.fit_h() : child->flex.fit_w();
    FlexItemInput item;
    item.key = static_cast<double>(targets.size());
    item.width = child->w();
    item.height = child->h();
    item.grow = child->flex.grow();
    item.shrink = child->flex.shrink();
    item.basis_auto = child->flex.basis_auto();
    item.basis = child->flex.basis();
    item.align_self = child->flex.align_self();
    item.order = child->flex.order();
    item.stretchable = (child->stretchable || child->flex.stretch_force()) && !fits_cross;
    items.push_back(item);
    targets.push_back(child);
  }
  if (items.empty()) return;

  if ((flex.fit_w() || flex.fit_h()) && flex.wrap() == u"nowrap") {
    const double main_gap = std::max(0.0, is_row ? flex.col_gap() : flex.row_gap());
    double main_sum = 0.0;
    double cross_max = 0.0;
    for (size_t i = 0; i < items.size(); ++i) {
      const FlexItemInput& item = items[i];
      const double main = item.basis_auto ? (is_row ? item.width : item.height) : item.basis;
      const double cross = is_row ? item.height : item.width;
      main_sum += main + (i != 0 ? main_gap : 0.0);
      cross_max = std::max(cross_max, cross);
    }
    const double w = (is_row ? main_sum : cross_max) + flex.padding().left + flex.padding().right;
    const double h = (is_row ? cross_max : main_sum) + flex.padding().top + flex.padding().bottom;
    const double target_w = flex.fit_w() ? w : node.w();
    const double target_h = flex.fit_h() ? h : node.h();
    if (node.w() != target_w || node.h() != target_h) node.resize(target_w, target_h);
  }

  FlexContainerInput container;
  container.width = node.w();
  container.height = node.h();
  container.direction = flex.direction();
  container.wrap = flex.wrap();
  container.justify = flex.justify();
  container.align = flex.align();
  container.align_content = flex.align_content();
  container.row_gap = flex.row_gap();
  container.col_gap = flex.col_gap();
  container.padding = flex.padding();

  const std::vector<FlexItemOutput> result = compute_flex_layout(container, items);
  const double origin_x = -node.cx() * node.w() + flex.padding().left;
  const double origin_y = -node.cy() * node.h() + flex.padding().top;
  for (const FlexItemOutput& r : result) {
    const size_t key = static_cast<size_t>(r.key);
    if (key >= targets.size()) continue;
    UINode* const child = targets[key];
    const double w = round_float(r.width);
    const double h = round_float(r.height);
    if (child->w() != w || child->h() != h) child->resize(w, h);
    const double x = round_float(origin_x + r.x + child->cx() * child->w());
    const double y = round_float(origin_y + r.y + child->cy() * child->h());
    if (child->x() != x || child->y() != y) child->move_to(x, y, child->z());
  }
}

}  // namespace lfw::ui
