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
};

struct UIPopPageOpts {
  bool inclusive = false;
  std::function<bool(UINode&, double, const std::vector<UINode*>&)> until;
  double min_pages = 0.0;
};

struct UIPushPageOpts {
  std::u16string id;
  bool has_id = false;
};

class UILayer {
 public:
  UILayer(LFW& lfw, double index);
  ~UILayer();

  LFW& lfw() const { return *_lfw; }
  double index() const { return _index; }
  std::vector<UINode*> pages() const;
  UINode* ui() const;
  UINode* at(double idx) const;

  UILayersCallbacks callbacks;

  void dispose();
  void set(const UIPushPageOpts& opts);
  void push(const UIPushPageOpts& opts);
  void pop(const UIPopPageOpts& opts);

 private:
  std::unique_ptr<UINode> create_page(const UIPushPageOpts& opts);
  LFW* _lfw = nullptr;
  double _index = 0;
  std::vector<std::unique_ptr<UINode>> _pages;
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
  void dispose();
  UINode* ui();
  std::vector<UINode*> layer_nodes();

 private:
  static UIPushPageOpts to_opts(const Value& opts);
  LFW* _lfw = nullptr;
  std::vector<std::unique_ptr<UILayer>> _all;
  std::vector<std::unique_ptr<UILayer>> _loose;
};

}
}
