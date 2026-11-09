#pragma once

#include <functional>
#include <string>

#include "lfw/core/value.h"

namespace lfw::ui {

/** 四边内边距：上、右、下、左（取值规则同 CSS padding） */
struct FlexPadding {
  double top = 0.0;
  double right = 0.0;
  double bottom = 0.0;
  double left = 0.0;
};

/**
 * 节点的 flex 声明
 *
 * 对应 UI 数据里的 `flex`（`"display:flex; direction:column; gap:6"` 或对象写法）。
 * 归一化后以字符串保存，布局阶段直接按值比较。
 */
class FlexStyle {
 public:
  std::function<void()> on_change;

  FlexStyle() = default;
  explicit FlexStyle(const Value& data) { assign(data); }

  /** 用 `flex` 数据覆盖当前声明；数据既不是字符串也不是对象时保持原值 */
  void assign(const Value& data);

  bool is_flex() const { return _display == u"flex"; }

  const std::u16string& display() const { return _display; }
  const std::u16string& direction() const { return _direction; }
  const std::u16string& wrap() const { return _wrap; }
  const std::u16string& justify() const { return _justify; }
  const std::u16string& align() const { return _align; }
  const std::u16string& align_content() const { return _align_content; }
  const std::u16string& align_self() const { return _align_self; }
  double gap() const { return _gap; }
  double row_gap() const { return _row_gap; }
  double col_gap() const { return _col_gap; }
  const FlexPadding& padding() const { return _padding; }
  double grow() const { return _grow; }
  double shrink() const { return _shrink; }
  bool basis_auto() const { return _basis_auto; }
  double basis() const { return _basis; }
  double order() const { return _order; }
  bool fit_w() const { return _fit_w; }
  bool fit_h() const { return _fit_h; }
  bool fit() const { return _fit_w && _fit_h; }
  bool stretch_force() const { return _stretch_force; }

  void set_stretch_force(bool v) {
    if (v == _stretch_force) return;
    _stretch_force = v;
    if (on_change) on_change();
  }

  void set_display(const std::u16string& v);
  void set_direction(const std::u16string& v);
  void set_align(const std::u16string& v);
  void set_row_gap(double v);
  void set_col_gap(double v);
  void set_shrink(double v);

 private:
  std::u16string _display = u"block";
  std::u16string _direction = u"row";
  std::u16string _wrap = u"nowrap";
  std::u16string _justify = u"flex-start";
  std::u16string _align = u"stretch";
  std::u16string _align_content = u"stretch";
  std::u16string _align_self = u"auto";
  double _gap = 0.0;
  double _row_gap = 0.0;
  double _col_gap = 0.0;
  FlexPadding _padding;
  double _grow = 0.0;
  double _shrink = 1.0;
  bool _basis_auto = true;
  double _basis = 0.0;
  double _order = 0.0;
  bool _fit_w = false;
  bool _fit_h = false;
  bool _stretch_force = false;
};

}
