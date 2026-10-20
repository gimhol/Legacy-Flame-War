#pragma once

#include <string>
#include <vector>

#include "lfw/ui/component/picture.h"

namespace lfw {

class PlayerInfo;

namespace ui {

class FighterHead : public Picture {
 public:
  static constexpr const char* TAG = "FighterHead";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  using Picture::Picture;

  const ClazzTag* clazz() const override { return class_tag(); }

  std::u16string player_id();
  PlayerInfo* player();
  std::u16string head();

  void on_start() override;
  void handle_changed();
};

}
}
