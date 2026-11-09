#include "lfw/ui/component/fighter_name.h"

#include <string>

#include "lfw/lfw.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"

namespace lfw {
namespace ui {

const ClazzTag* FighterName::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& FighterName::TAGS() {
  static const std::vector<std::u16string> tags{u"FighterName"};
  return tags;
}

/** 字号等静态样式由 UI 文件负责，这里只按 COM / 玩家切换颜色 */
void FighterName::join(const std::u16string& text, bool com, bool decided) {
  _decided = decided;
  _com = com;
  node.style.set_fill_style(Value(std::u16string(com ? u"pink" : u"white")));
  node.set_text(text);
  node.set_visible(true);
}

void FighterName::quit() {
  _com = false;
  _decided = false;
  const std::u16string text = to_string(lfw().string(Value(std::u16string(u" "))));
  node.style.set_fill_style(Value(std::u16string(u"white")));
  node.set_text(text);
  node.set_visible(false);
}

void FighterName::update(double dt) {
  _opacity.update(dt);
  node.set_opacity(_decided ? 1.0 : _opacity.value());
}

}
}
