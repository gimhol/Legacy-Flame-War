#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/lfw.h"

namespace lfw {

namespace ui {

class UINode;
class UILayer;

struct UILayersCallbacks {
  std::function<void(UINode*, UINode*, UILayer&)> on_set;
  std::function<void(UINode*, UINode*, UILayer&)> on_push;
  std::function<void(UINode*, const std::vector<UINode*>&, UILayer&)> on_pop;
  /** 打开弹窗：`opened` 是弹窗，`prev` 是打开前的层顶节点。 */
  std::function<void(UINode*, UINode*, UILayer&)> on_modal_open;
  /** 关闭弹窗：`curr` 是关闭后的层顶节点，`closed` 末尾为最上层弹窗。 */
  std::function<void(UINode*, const std::vector<UINode*>&, UILayer&)> on_modal_close;
};

struct UIPopPageOpts {
  bool inclusive = false;
  std::function<bool(UINode&, double, const std::vector<UINode*>&)> until;
  double min_pages = 0.0;
};

struct UIPushPageOpts {
  std::u16string id;
  bool has_id = false;
  std::u16string transition;
};

struct UICloseModalOpts {
  /** 关闭到该 id 的弹窗（含）；缺省只关最上层一个。 */
  std::u16string id;
  bool has_id = false;
};

class UILayer {
 public:
  /** 弹窗相对所属页面抬高的 z 差（小于 1，不会压到上一层）。 */
  static constexpr double MODAL_Z_STEP = 0.25;

  UILayer(LFW& lfw, double index);
  ~UILayer();

  LFW& lfw() const { return *_lfw; }
  double index() const { return _index; }
  /** 本层节点栈（页面 + 弹窗，末尾为当前节点）。 */
  std::vector<UINode*> pages() const;
  /** 本层最上层节点（有弹窗时为最上层弹窗）。 */
  UINode* ui() const;
  /** 本层当前页面（忽略盖在上面的弹窗）。 */
  UINode* page() const;
  UINode* at(double idx) const;

  UINode* modal() const;
  const std::vector<UINode*> modals() const;
  bool has_modal() const;
  bool has_modal(const std::u16string& id) const;
  /** 需要更新/绘制的节点数：当前页面（若有）+ 其上所有弹窗。 */
  double live_count() const;
  /** 第 i 个需要更新/绘制的节点（0 = 当前页面，往后依次为弹窗）。 */
  UINode* live_at(double i) const;

  UILayersCallbacks callbacks;

  void dispose();
  void set(const UIPushPageOpts& opts);
  void push(const UIPushPageOpts& opts);
  /** 弹掉页面（本层弹窗随之关闭）；返回是否真的弹了。 */
  bool pop(const UIPopPageOpts& opts);
  /** 在当前页面上打开弹窗；找不到 UI 数据时返回 `nullptr`。 */
  UINode* open_modal(const UIPushPageOpts& opts);
  /** 关闭弹窗：缺省关最上层一个；带 `id` 时自顶向下关到（含）该弹窗。 */
  bool close_modal(const UICloseModalOpts& opts);
  /** 关闭本层全部弹窗。 */
  void close_modals();

 private:
  std::unique_ptr<UINode> create_page(const UIPushPageOpts& opts);
  LFW* _lfw = nullptr;
  double _index = 0;
  std::vector<std::unique_ptr<UINode>> _pages;
  std::vector<UINode*> _modals;
  std::vector<std::unique_ptr<UINode>> _trash;
};

class UILayers {
 public:
  explicit UILayers(LFW& lfw);

  LFW& lfw() const { return *_lfw; }
  UILayer* bottom() const;
  UILayer* top() const;
  std::vector<UILayer*> all() const;
  UINode* ui_top() const;
  double length() const;
  UILayer& push_layer();
  UILayer& ensure(double index);
  UILayer* at(double index);

  void push();
  void set_page(const Value& opts, double index);
  void push_page(const Value& opts, double index);
  void pop_page(const Value& opts, double index);
  /** 在第 index 层的当前页面上打开弹窗 / 关闭弹窗。 */
  UINode* open_modal(const Value& opts, double index);
  bool close_modal(const Value& opts, double index);
  bool has_modal() const;
  void dispose();
  UINode* ui();
  std::vector<UINode*> layer_nodes();
  /** 各层最上层节点，自顶向下（供指针命中按层筛选）。 */
  std::vector<UINode*> layer_tops() const;
  /**
   * 页面栈代数：任何 push/pop/set/弹窗都会 +1。
   * 宿主用它判断「上一帧渲染时收集的 UI 指针」是否还代表当前界面
   * （页面被换掉后那些 `UINode*`/`TextInput*` 已经析构，不能再碰）。
   */
  int generation() const { return _generation; }
  void bump_generation() { ++_generation; }

 private:
  static UIPushPageOpts to_opts(const Value& opts);
  /** 页面栈的「形状」（各层页数 + 栈顶 id）；只有真变了才 `bump_generation()` */
  std::u16string shape() const;
  LFW* _lfw = nullptr;
  std::vector<std::unique_ptr<UILayer>> _all;
  std::vector<std::unique_ptr<UILayer>> _loose;
  int _generation = 0;
};

}
}
