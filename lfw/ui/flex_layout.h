#pragma once

#include <string>
#include <vector>

#include "lfw/ui/flex_style.h"

namespace lfw::ui {

class UINode;

/** 单个子项参与布局的输入 */
struct FlexItemInput {
  double key = 0.0;
  double width = 0.0;
  double height = 0.0;
  double grow = 0.0;
  double shrink = 1.0;
  bool basis_auto = true;
  double basis = 0.0;
  std::u16string align_self = u"auto";
  double order = 0.0;
  bool stretchable = false;
};

/** 容器参与布局的输入 */
struct FlexContainerInput {
  double width = 0.0;
  double height = 0.0;
  std::u16string direction = u"row";
  std::u16string wrap = u"nowrap";
  std::u16string justify = u"flex-start";
  std::u16string align = u"stretch";
  std::u16string align_content = u"stretch";
  double row_gap = 0.0;
  double col_gap = 0.0;
  FlexPadding padding;
};

/** 单个子项的布局结果 */
struct FlexItemOutput {
  double key = 0.0;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
};

/** 纯计算部分：与 TS 的 `compute_flex_layout` 一一对应 */
std::vector<FlexItemOutput> compute_flex_layout(const FlexContainerInput& container,
                                                const std::vector<FlexItemInput>& items);

/** 按容器自身的 `flex` 声明给可见子节点排版（会改子节点 size/pos） */
void apply_flex_layout(UINode& node);

}
