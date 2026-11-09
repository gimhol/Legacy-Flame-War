#include "lfw/ui/component/player_score_player_name.h"

#include <string>
#include <vector>

#include "lfw/base/team_color.h"
#include "lfw/defines/team_enum.h"
#include "lfw/entity/entity.h"
#include "lfw/ui/component/player_score.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

const ClazzTag* PlayerScorePlayerName::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& PlayerScorePlayerName::TAGS() {
  static const std::vector<std::u16string> tags{u"PlayerScorePlayerName"};
  return tags;
}

void PlayerScorePlayerName::on_show() {
  UIComponent* const c = node.lookup_component(PlayerScore::class_tag());
  PlayerScore* const ps =
      c != nullptr && is_class(c->clazz(), PlayerScore::class_tag())
          ? static_cast<PlayerScore*>(c)
          : nullptr;
  Entity* const fighter = ps != nullptr ? ps->fighter() : nullptr;
  const std::u16string text = fighter != nullptr ? to_string(fighter->name()) : u"-";
  const std::u16string team = fighter != nullptr ? fighter->team() : std::u16string();
  const Object* team_info_obj = team_color_detail::team_info(team);
  if (team_info_obj == nullptr) {
    team_info_obj = team_color_detail::team_info(std::u16string(team_enum::kIndependent));
  }
  const std::u16string* const toc =
      team_color_detail::team_field(team_info_obj, u"txt_outline_color");
  const std::u16string* const tc = team_color_detail::team_field(team_info_obj, u"txt_color");
  const std::u16string outline_color = toc != nullptr ? *toc : std::u16string();
  const std::u16string txt_color = tc != nullptr ? *tc : std::u16string();
  node.set_outline_color(Value(outline_color));
  // 字号、描边宽度/透明度在 UI 里，这里只按队伍换颜色
  node.style.set_fill_style(Value(txt_color));
  node.set_text(text);
}

}
}
