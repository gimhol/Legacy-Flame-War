#include "lfw/ui/component/main_menu_logic.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/reachable_group.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

struct MainMenuLogic::DPtr {
  explicit DPtr(MainMenuLogic& owner) : self(owner) {}

  std::vector<UINode*>& survival_btns();
  std::vector<UINode*>& danmu_btns();
  ReachableGroup* reachable_group();

  MainMenuLogic& self;
  std::optional<std::vector<UINode*>> survival_btns_cache;
  std::optional<std::vector<UINode*>> danmu_btns_cache;
};

const ClazzTag* MainMenuLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& MainMenuLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"MainMenuLogic"};
  return tags;
}

MainMenuLogic::MainMenuLogic(UINode& layout, const std::u16string& f_name, const Value& info)
    : UIComponent(layout, f_name, info), dptr(std::make_unique<DPtr>(*this)) {}

MainMenuLogic::~MainMenuLogic() = default;

std::vector<UINode*>& MainMenuLogic::DPtr::survival_btns() {
  if (!survival_btns_cache.has_value()) {
    const std::u16string ids[] = {u"btn_survival", u"btn_survival_2p"};
    std::vector<UINode*> found;
    for (const std::u16string& id : ids) {
      UINode* const n = self.node.search_node(id);
      if (n != nullptr) found.push_back(n);
    }
    survival_btns_cache = std::move(found);
  }
  return *survival_btns_cache;
}

std::vector<UINode*>& MainMenuLogic::DPtr::danmu_btns() {
  if (!danmu_btns_cache.has_value()) {
    UINode* const btn = self.node.search_node(u"btn_danmu_mode");
    std::vector<UINode*> found;
    if (btn != nullptr) found.push_back(btn);
    danmu_btns_cache = std::move(found);
  }
  return *danmu_btns_cache;
}

ReachableGroup* MainMenuLogic::DPtr::reachable_group() {
  UIComponent* const c = self.node.search_component(ReachableGroup::class_tag());
  if (c == nullptr) return nullptr;
  return is_class(c->clazz(), ReachableGroup::class_tag()) ? static_cast<ReachableGroup*>(c)
                                                           : nullptr;
}

void MainMenuLogic::apply() {
  DPtr& d = *dptr;
  const bool available = lfw().survival_rank_available();
  for (UINode* const btn : d.survival_btns()) {
    btn->set_visible(available);
    btn->set_disabled(!available);
  }
  const bool danmu_available = lfw().danmu_available;
  for (UINode* const btn : d.danmu_btns()) {
    btn->set_visible(danmu_available);
    btn->set_disabled(!danmu_available);
  }
  if (!available && node.focused_node() == nullptr) {
    ReachableGroup* const g = d.reachable_group();
    if (g != nullptr) g->focus_next();
  }
}

void MainMenuLogic::on_start() {
  UIComponent::on_start();
  apply();
}

void MainMenuLogic::on_resume() {
  UIComponent::on_resume();
  apply();
}

}
}
