#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ui/component/sum_info.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/world.h"

namespace lfw {
namespace ui {

class SummaryLogic;

class TeamSituationText : public UIComponent {
 public:
  static constexpr const char* TAG = "TeamSituationText";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  explicit TeamSituationText(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;
  void on_resume() override;
  void update(double dt) override;

  void set_team(const std::u16string& team);
  void rr();

 protected:
  std::optional<std::u16string> _team;
  /** 队伍色随数据变化；字号、描边等静态样式在 UI 文件里 */
  std::u16string _team_color = u"white";
  std::u16string _color;
  SummaryLogic* _sum_logic = nullptr;
  std::u16string _sum_team;
  std::u16string _text = u" ";

 private:
  using WorldListener = NoEmitCallbacksT<WorldCallbackArgs>::Listener;
  using WorldPayloads = NoEmitCallbacksT<WorldCallbackArgs>::Payloads;

  struct StageListener : WorldListener {
    explicit StageListener(TeamSituationText* o);
  };

  StageListener _world_cb{this};
};

}
}
