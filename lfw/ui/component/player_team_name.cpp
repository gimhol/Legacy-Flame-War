#include "lfw/ui/component/player_team_name.h"

#include <string>

#include "lfw/lfw.h"
#include "lfw/ui/uinode.h"

namespace lfw {
namespace ui {

const ClazzTag* PlayerTeamName::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& PlayerTeamName::TAGS() {
  static const std::vector<std::u16string> tags{u"PlayerTeamName"};
  return tags;
}

/** 字号等静态样式在 UI 里，这里只按 COM / 玩家换颜色 */
void PlayerTeamName::join(const std::u16string& text, bool com, bool decided) {
  _decided = decided;
  _com = com;
  node.style.set_fill_style(Value(std::u16string(com ? u"pink" : u"white")));
  node.set_text(text);
  node.set_visible(true);
}

void PlayerTeamName::quit() {
  _decided = false;
  _com = false;
  const std::u16string text = to_string(lfw().string(Value(std::u16string(u" "))));
  node.style.set_fill_style(Value(std::u16string(u"white")));
  node.set_text(text);
  node.set_visible(false);
}

void PlayerTeamName::update(double dt) {
  _opacity.update(dt);
  node.set_opacity(_decided ? 1.0 : _opacity.value());
}

}
}
