#include "lfw/ui/component/jalousie.h"

#include "lfw/ui/uinode.h"
#include "lfw/utils/type_check.h"

namespace lfw {
namespace ui {

namespace {

bool falsy_num(double d) { return !(d != 0.0); }

}

const ClazzTag* Jalousie::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& Jalousie::TAGS() {
  static const std::vector<std::u16string> tags{u"Jalousie"};
  return tags;
}

std::vector<Animation*> Jalousie::make_anims(Easing& e1, Sine& s1, Easing& e2) {
  e1.set_duration(250.0);
  s1.set_duration(250.0);
  e2.set_duration(250.0);
  return {&e1, &s1, &e2};
}

bool Jalousie::click_test() {
  const std::optional<bool> v = props_holder.bool_(u"click_test");
  return v.has_value() && *v;
}

std::u16string Jalousie::direction() {
  return props_holder.str(u"direction", {u"row", u"column"}).value_or(u"row");
}

bool Jalousie::open() const { return _anim.reverse(); }

void Jalousie::set_open(bool v) { _anim.start(v); }

double Jalousie::w() const { return node.root().w(); }

double Jalousie::h() const { return node.root().h(); }

void Jalousie::apply_layout() {
  // 百叶窗本身就是一个 flex 容器：子项沿 direction 排开、撑满交叉轴、不收缩
  FlexStyle& flex = node.flex;
  flex.set_display(u"flex");
  flex.set_direction(direction());
  flex.set_align(u"stretch");
  flex.set_row_gap(0.0);
  flex.set_col_gap(0.0);
  for (UINode* const child : node.children()) {
    if (child == nullptr) continue;
    child->flex.set_shrink(0.0);
    child->flex.set_stretch_force(true);
  }
  node.invalidate_flex();
  node.flush_flex();
}

void Jalousie::on_start() {
  sync_screen_rect();
  apply_layout();
  const std::optional<bool> open_v = props_holder.bool_(u"open");
  const std::optional<bool> end_v = props_holder.bool_(u"end");
  const bool open = open_v.has_value() ? *open_v : bool_(1);
  const bool end = end_v.has_value() ? *end_v : bool_(2);
  if (end) {
    _anim.end(open);
  } else {
    _anim.start(open);
  }
}

void Jalousie::on_resume() {
  sync_screen_rect();
  apply_layout();
}

void Jalousie::on_click(LFWPointerEvent& e) {
  (void)e;
  if (click_test()) set_open(!open());
}

void Jalousie::on_stop() { callbacks.clear(); }

void Jalousie::update(double dt) {
  if (_anim.done()) return;
  _anim.update(dt);
  update_children();
  if (_anim.done()) callbacks.call(u"on_anim_end", {JalousieCallbackArgs{this}});
}

void Jalousie::on_show() {
  _anim.calc();
  update_children();
}

void Jalousie::sync_screen_rect() {
  UINode& root = node.root();
  if (falsy_num(root.w()) || falsy_num(root.h())) return;
  const double cx = node.center.x;
  const double cy = node.center.y;
  node.move_to_global(root.global_pos().x + cx * root.w(),
                      root.global_pos().y + cy * root.h(), node.global_pos().z);
  node.resize(root.w(), root.h());
}

void Jalousie::update_children() {
  const double value = _anim.value();
  const std::u16string dir = direction();
  const std::vector<UINode*>& kids = node.children();
  const size_t len = kids.size();
  for (size_t i = 0; i < len; ++i) {
    UINode* const child = kids[i];
    if (child == nullptr) continue;
    if (dir == u"column") {
      child->set_scale(1.0, value, 1.0);
    } else {
      child->set_scale(value, 1.0, 1.0);
    }
  }
}

}
}
