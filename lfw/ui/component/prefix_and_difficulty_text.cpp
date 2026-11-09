#include "lfw/ui/component/prefix_and_difficulty_text.h"

#include "lfw/defines/difficulty.h"
#include "lfw/ui/uinode.h"
#include "lfw/world.h"

namespace lfw {
namespace ui {

const ClazzTag* PrefixAndDifficultyText::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& PrefixAndDifficultyText::TAGS() {
  static const std::vector<std::u16string> tags{u"PrefixAndDifficultyText"};
  return tags;
}

std::u16string PrefixAndDifficultyText::text() {
  std::u16string out = _prefix;
  out += u" (";
  out += difficulty_full_name(world().dataset.difficulty());
  out += u")";
  return out;
}

void PrefixAndDifficultyText::on_start() {
  _prefix = str(0).value_or(std::u16string());
}

void PrefixAndDifficultyText::on_resume() {
  if (!_remover) {
    _remover = world().callbacks.on(
        u"on_dataset_change", [this](const WorldCallbacks::Payloads& p) {
          for (const WorldCallbackArgs& a : p) on_dataset_change(a.key);
        });
  }
  on_dataset_change(u"difficulty");
}

void PrefixAndDifficultyText::on_pause() {
  if (_remover) {
    _remover();
    _remover = nullptr;
  }
}

/** 文字样式由 UI 文件里的 style 决定，这里只负责内容 */
void PrefixAndDifficultyText::on_dataset_change(const std::u16string& key) {
  if (key != u"difficulty") return;
  node.set_text(text());
}

}
}
