#include "lfw/ui/uilayer.h"

#include <algorithm>
#include <utility>

#include "lfw/helper/ui_helper.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/math/base.h"
#include "lfw/world.h"

namespace lfw::ui {

UILayer::UILayer(LFW& lfw, double index) : _lfw(&lfw), _index(index) {}

std::vector<UINode*> UILayer::pages() const {
  std::vector<UINode*> ret;
  for (const std::unique_ptr<UINode>& p : _pages) {
    if (p != nullptr) ret.push_back(p.get());
  }
  return ret;
}

UINode* UILayer::ui() const { return _pages.empty() ? nullptr : _pages.back().get(); }

UINode* UILayer::page() const {
  const size_t k = _pages.size() - std::min(_pages.size(), _modals.size());
  return k == 0 ? nullptr : _pages[k - 1].get();
}

UINode* UILayer::modal() const { return _modals.empty() ? nullptr : _modals.back(); }

const std::vector<UINode*> UILayer::modals() const { return _modals; }

bool UILayer::has_modal() const { return !_modals.empty(); }

bool UILayer::has_modal(const std::u16string& id) const {
  const Value want(id);
  for (UINode* const m : _modals) {
    if (m != nullptr && strict_equals(m->id(), want)) return true;
  }
  return false;
}

double UILayer::live_count() const {
  return static_cast<double>(_modals.size()) + (_pages.size() > _modals.size() ? 1.0 : 0.0);
}

UINode* UILayer::live_at(double i) const {
  if (!(i >= 0) || i != floor(i)) return nullptr;
  const size_t k = static_cast<size_t>(live_count() - i);
  if (k == 0 || k > _pages.size()) return nullptr;
  return _pages[_pages.size() - k].get();
}

UINode* UILayer::at(double idx) const {
  if (!(idx >= 0) || idx != floor(idx)) return nullptr;
  const size_t i = static_cast<size_t>(idx);
  if (i >= _pages.size()) return nullptr;
  return _pages[i].get();
}

std::unique_ptr<UINode> UILayer::create_page(const UIPushPageOpts& opts) {
  const Value want = opts.has_id ? Value(opts.id) : Value();
  for (const Value& v : _lfw->ui_helper().all()) {
    if (strict_equals(field_or(v, u"id"), want)) {
      return UINode::create(*_lfw, v, nullptr, this);
    }
  }
  // 页面 id 找不到时以前是静默返回：数据包过期（新页面还没打进 prel）时表现为
  // 「点了按钮没反应」，很难查。这里至少留一条日志。
  if (opts.has_id) {
    _lfw->warn(u"[UILayer] ui not found: " + opts.id +
               u"（数据包可能过期：确认 --zip 指向最新的 dist/prel.zip 或 lf2s/origin/prel_data）");
  }
  return nullptr;
}

void UILayer::dispose() {
  for (std::unique_ptr<UINode>& p : _pages) {
    if (p == nullptr) continue;
    p->on_pause();
    p->on_stop();
  }
  _modals.clear();
}

void UILayer::set(const UIPushPageOpts& opts) {
  _trash.clear();
  close_modals();
  if (opts.has_id && page() != nullptr) {
    const Value idv = page()->id();
    const std::u16string* const cur = std::get_if<std::u16string>(&idv);
    if (cur != nullptr && *cur == opts.id) return;
  }
  std::unique_ptr<UINode> prev;
  if (!_pages.empty()) {
    prev = std::move(_pages.back());
    _pages.pop_back();
    if (prev != nullptr) {
      prev->on_pause();
      prev->on_stop();
    }
  }
  std::unique_ptr<UINode> curr = create_page(opts);
  UINode* curr_raw = nullptr;
  if (curr != nullptr) {
    curr->set_z(curr->z() + _index);
    _pages.push_back(std::move(curr));
    curr_raw = _pages.back().get();
    curr_raw->on_start();
    curr_raw->on_resume();
  }
  if (curr_raw != nullptr || prev != nullptr) {
    if (callbacks.on_set) callbacks.on_set(curr_raw, prev.get(), *this);
  }
  if (prev != nullptr) _trash.push_back(std::move(prev));
}

void UILayer::push(const UIPushPageOpts& opts) {
  _trash.clear();
  close_modals();
  UINode* const prev = ui();
  if (prev != nullptr) prev->on_pause();
  std::unique_ptr<UINode> curr = create_page(opts);
  UINode* curr_raw = nullptr;
  if (curr != nullptr) {
    curr->set_z(curr->z() + _index);
    _pages.push_back(std::move(curr));
    curr_raw = _pages.back().get();
    curr_raw->on_start();
    curr_raw->on_resume();
  }
  if (callbacks.on_push) callbacks.on_push(curr_raw, prev, *this);
}

bool UILayer::pop(const UIPopPageOpts& opts) {
  _trash.clear();
  close_modals();
  std::vector<UINode*> poppeds;
  const size_t len = _pages.size();
  const double max_pop =
      static_cast<double>(len) - min(max(opts.min_pages, 0.0), static_cast<double>(len));
  for (double i = static_cast<double>(len) - 1.0;
       i >= 0 && static_cast<double>(poppeds.size()) < max_pop; --i) {
    UINode* const one = _pages[static_cast<size_t>(i)].get();
    if (!opts.until) {
      poppeds.push_back(one);
      break;
    }
    if (opts.until(*one, i, pages())) {
      if (opts.inclusive) poppeds.push_back(one);
      break;
    }
    poppeds.push_back(one);
  }
  if (poppeds.empty()) return false;
  for (size_t i = 0; i < poppeds.size(); ++i) {
    if (i == 0) poppeds[i]->on_pause();
    poppeds[i]->on_stop();
  }
  for (size_t i = len - poppeds.size(); i < len; ++i) {
    _trash.push_back(std::move(_pages[i]));
  }
  _pages.erase(_pages.begin() + static_cast<std::ptrdiff_t>(len - poppeds.size()), _pages.end());
  if (ui() != nullptr) ui()->on_resume();
  if (callbacks.on_pop) callbacks.on_pop(ui(), poppeds, *this);
  return true;
}

UINode* UILayer::open_modal(const UIPushPageOpts& opts) {
  if (opts.has_id && modal() != nullptr) {
    const Value mid = modal()->id();
    const std::u16string* const cur = std::get_if<std::u16string>(&mid);
    if (cur != nullptr && *cur == opts.id) return modal();
  }
  UINode* const prev = ui();
  std::unique_ptr<UINode> curr = create_page(opts);
  if (curr == nullptr) return nullptr;
  curr->set_z(curr->z() + MODAL_Z_STEP * static_cast<double>(_modals.size() + 1));
  UINode* const raw = curr.get();
  _pages.push_back(std::move(curr));
  _modals.push_back(raw);
  raw->on_start();
  raw->on_resume();
  if (callbacks.on_modal_open) callbacks.on_modal_open(raw, prev, *this);
  return raw;
}

bool UILayer::close_modal(const UICloseModalOpts& opts) {
  if (_modals.empty()) return false;
  if (opts.has_id && !has_modal(opts.id)) return false;
  std::vector<UINode*> closed;
  while (!_modals.empty()) {
    UINode* const m = _modals.back();
    _modals.pop_back();
    if (!_pages.empty()) {
      _trash.push_back(std::move(_pages.back()));
      _pages.pop_back();
    }
    closed.push_back(m);
    if (!opts.has_id) break;
    const Value want(opts.id);
    if (m != nullptr && strict_equals(m->id(), want)) break;
  }
  for (size_t i = 0; i < closed.size(); ++i) {
    if (closed[i] == nullptr) continue;
    if (i == 0) closed[i]->on_pause();
    closed[i]->on_stop();
  }
  if (callbacks.on_modal_close) callbacks.on_modal_close(ui(), closed, *this);
  return true;
}

void UILayer::close_modals() {
  if (_modals.empty()) return;
  UICloseModalOpts opts;
  const Value bottom_id = _modals.front()->id();
  const std::u16string* const bottom = std::get_if<std::u16string>(&bottom_id);
  if (bottom != nullptr) {
    opts.id = *bottom;
    opts.has_id = true;
  }
  close_modal(opts);
}

UILayers::UILayers(LFW& lfw) : _lfw(&lfw) {}

UILayer::~UILayer() = default;

UILayer* UILayers::bottom() const { return _all.empty() ? nullptr : _all.front().get(); }

UILayer* UILayers::top() const { return _all.empty() ? nullptr : _all.back().get(); }

std::vector<UILayer*> UILayers::all() const {
  std::vector<UILayer*> ret;
  for (const std::unique_ptr<UILayer>& l : _all) ret.push_back(l.get());
  return ret;
}

UINode* UILayers::ui_top() const {
  for (size_t i = _all.size(); i > 0; --i) {
    UILayer* const layer = _all[i - 1].get();
    if (layer == nullptr) continue;
    if (UINode* const u = layer->ui()) return u;
  }
  return nullptr;
}

double UILayers::length() const { return static_cast<double>(_all.size()); }

UILayer& UILayers::push_layer() {
  _all.push_back(std::make_unique<UILayer>(*_lfw, 0.0));
  return *_all.back();
}

UILayer& UILayers::ensure(double index) {
  if (!(index >= 0) || index != floor(index)) {
    _loose.push_back(std::make_unique<UILayer>(*_lfw, index));
    return *_loose.back();
  }
  const size_t i = static_cast<size_t>(index);
  if (i >= _all.size()) _all.resize(i + 1);
  if (_all[i] == nullptr) _all[i] = std::make_unique<UILayer>(*_lfw, index);
  return *_all[i];
}

UILayer* UILayers::at(double index) {
  if (!(index >= 0) || index != floor(index)) return nullptr;
  const size_t i = static_cast<size_t>(index);
  if (i >= _all.size()) return nullptr;
  return _all[i].get();
}

void UILayers::push() {
  push_layer();
  bump_generation();
}

std::u16string UILayers::shape() const {
  std::u16string out;
  for (const std::unique_ptr<UILayer>& layer : _all) {
    if (layer == nullptr) {
      out += u"-;";
      continue;
    }
    const UINode* const top = layer->ui();
    const Value id = top != nullptr ? top->id() : Value();
    const std::u16string* const id_s = std::get_if<std::u16string>(&id);
    out += number_to_string(layer->index()) + u"|" +
           number_to_string(static_cast<double>(layer->pages().size())) + u"|" +
           (id_s != nullptr ? *id_s : std::u16string()) + u";";
  }
  return out;
}

void UILayers::set_page(const Value& opts, double index) {
  // 只有页面栈真的变了才 +1：幂等的 set/pop 每帧都调的话，代数一直变会让
  // 「换页后挡住指针」变成永久挡（UI 就点不动了）。
  const std::u16string before = shape();
  ensure(index).set(to_opts(opts));
  if (shape() != before) bump_generation();
}

void UILayers::push_page(const Value& opts, double index) {
  const std::u16string before = shape();
  ensure(index).push(to_opts(opts));
  if (shape() != before) bump_generation();
}

void UILayers::pop_page(const Value& opts, double index) {
  UILayer* const layer = at(index);
  if (layer == nullptr) return;
  const std::u16string before = shape();
  UIPopPageOpts pop;
  const Value* const min_pages = field_of(opts, u"min_pages");
  if (min_pages != nullptr) pop.min_pages = to_number(*min_pages);
  const Value* const inclusive = field_of(opts, u"inclusive");
  if (inclusive != nullptr) pop.inclusive = truthy(*inclusive);
  layer->pop(pop);
  if (shape() != before) bump_generation();
}

UINode* UILayers::open_modal(const Value& opts, double index) {
  const std::u16string before = shape();
  UINode* const ret = ensure(index).open_modal(to_opts(opts));
  if (shape() != before) bump_generation();
  return ret;
}

bool UILayers::close_modal(const Value& opts, double index) {
  UILayer* const layer = at(index);
  if (layer == nullptr) return false;
  const std::u16string before = shape();
  UICloseModalOpts close;
  const Value* const id = field_of(opts, u"id");
  if (id != nullptr) {
    if (const std::u16string* const s = std::get_if<std::u16string>(id)) {
      close.id = *s;
      close.has_id = true;
    }
  }
  const bool closed = layer->close_modal(close);
  if (shape() != before) bump_generation();
  return closed;
}

bool UILayers::has_modal() const {
  for (const std::unique_ptr<UILayer>& l : _all) {
    if (l != nullptr && l->has_modal()) return true;
  }
  return false;
}

std::vector<UINode*> UILayers::layer_tops() const {
  std::vector<UINode*> ret;
  for (size_t i = _all.size(); i > 0; --i) {
    UILayer* const layer = _all[i - 1].get();
    if (layer == nullptr) continue;
    if (UINode* const u = layer->ui()) ret.push_back(u);
  }
  return ret;
}

void UILayers::dispose() {
  for (std::unique_ptr<UILayer>& l : _all) {
    if (l != nullptr) l->dispose();
  }
  _all.clear();
}

UINode* UILayers::ui() { return ui_top(); }

std::vector<UINode*> UILayers::layer_nodes() {
  std::vector<UINode*> ret;
  for (const std::unique_ptr<UILayer>& l : _all) {
    if (l == nullptr) continue;
    UINode* const u = l->ui();
    if (u == nullptr) continue;
    ret.push_back(u);
  }
  return ret;
}

UIPushPageOpts UILayers::to_opts(const Value& opts) {
  UIPushPageOpts ret;
  const Value* const id = field_of(opts, u"id");
  if (id != nullptr) {
    if (const std::u16string* const s = std::get_if<std::u16string>(id)) {
      ret.id = *s;
      ret.has_id = true;
    }
  }
  return ret;
}

}
