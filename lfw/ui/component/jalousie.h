#pragma once

#include <vector>

#include "lfw/animation/easing.h"
#include "lfw/animation/sequence.h"
#include "lfw/animation/sine.h"
#include "lfw/base/no_emit_callbacks.h"
#include "lfw/ui/component/flex.h"

namespace lfw {
namespace ui {

class Jalousie;

struct JalousieCallbackArgs {
  Jalousie* self = nullptr;
};
using JalousieCallbacks = CallbacksT<JalousieCallbackArgs>;

class Jalousie : public Flex {
 public:
  static constexpr const char* TAG = "Jalousie";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using Flex::Flex;

  const ClazzTag* clazz() const override { return class_tag(); }

  JalousieCallbacks callbacks;

  Sequence& anim() { return _anim; }
  bool click_test();
  bool open() const;
  void set_open(bool v);
  double w() const;
  double h() const;

  void on_start() override;
  void on_resume() override;
  void on_click(LFWPointerEvent& e) override;
  void on_stop() override;
  void update(double dt) override;
  void on_show() override;

 protected:
  void sync_screen_rect();
  void update_children();

 private:
  static std::vector<Animation*> make_anims(Easing& e1, Sine& s1, Easing& e2);

  Easing _e1{0.0, 0.0};
  Sine _s1{-1.0, 2.0, 1.0};
  Easing _e2{1.0, 1.0};
  Sequence _anim{make_anims(_e1, _s1, _e2)};
};

}
}
