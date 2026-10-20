#pragma once

#include <string>
#include <vector>

#include "lfw/ui/component/picture.h"

namespace lfw {

class Entity;

namespace ui {

class FighterThumb : public Picture {
 public:
  static constexpr const char* TAG = "FighterThumb";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using Picture::Picture;

  const ClazzTag* clazz() const override { return class_tag(); }

  Entity* fighter = nullptr;
  std::u16string thumb_url();

  void on_start() override;
  void on_show() override;
  void handle_changed();
};

}
}
