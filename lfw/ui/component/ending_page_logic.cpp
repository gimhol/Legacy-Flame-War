#include "lfw/ui/component/ending_page_logic.h"

#include <memory>
#include <string>
#include <vector>

#include "lfw/defines/difficulty.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/jalousie.h"
#include "lfw/ui/instance_ref.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

struct EndingPageLogic::DPtr {
  struct JalousieCbT : JalousieCallbacks::Listener {
    explicit JalousieCbT(EndingPageLogic* o);
  };

  explicit DPtr(EndingPageLogic& owner) : self(owner), cb(&owner) {}

  double difficulty();

  EndingPageLogic& self;
  double texts_idx = 0;
  std::vector<std::u16string> texts{
      u"old_ending_0_0", u"old_ending_0_1", u"old_ending_1",  u"old_ending_2",
      u"old_ending_3",   u"gim_ending_0",   u"gim_ending_1",   u"gim_ending_2",
      u"gim_ending_3",
  };
  JalousieCbT cb;
};

const ClazzTag* EndingPageLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& EndingPageLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"EndingPageLogic"};
  return tags;
}

const Value& EndingPageLogic::PROPS() {
  static const Value meta = [] {
    auto node_prop = [] {
      auto p = std::make_shared<Object>();
      p->set(u"type", Value(u"$cls:UINode"));
      return p;
    };
    auto jalousie_prop = [] {
      auto p = std::make_shared<Object>();
      p->set(u"type", Value(u"$cls:Jalousie"));
      return p;
    };
    auto o = std::make_shared<Object>();
    o->set(u"label", Value(node_prop()));
    o->set(u"jalousie", Value(jalousie_prop()));
    return Value(o);
  }();
  return meta;
}

EndingPageLogic::EndingPageLogic(UINode& layout, const std::u16string& f_name,
                                 const Value& info)
    : UIComponent(layout, f_name, info), dptr(std::make_unique<DPtr>(*this)) {}

EndingPageLogic::~EndingPageLogic() = default;

EndingPageLogic::DPtr::JalousieCbT::JalousieCbT(EndingPageLogic* o) {
  handlers.push_back({u"on_anim_end", [o](const JalousieCallbacks::Payloads& a) {
                        if (!a.empty() && a[0].self != nullptr) o->on_jalousie_anim_end(*a[0].self);
                      }});
}

UINode* EndingPageLogic::label_node() {
  if (!ensure_props()) return nullptr;
  return ref_to_node(props_holder.prop_instance(u"label"));
}

Jalousie* EndingPageLogic::jalousie() {
  if (!ensure_props()) return nullptr;
  UIComponent* const c = ref_to_comp(props_holder.prop_instance(u"jalousie"));
  if (c == nullptr) return nullptr;
  return is_class(c->clazz(), Jalousie::class_tag()) ? static_cast<Jalousie*>(c) : nullptr;
}

double EndingPageLogic::DPtr::difficulty() {
  return to_number(field_or(self.world().world_dataset(), u"difficulty"));
}

void EndingPageLogic::on_start() {
  UIComponent::on_start();
  dptr->texts_idx = -1;
  Jalousie* const j = jalousie();
  if (j != nullptr) j->callbacks.add(&dptr->cb);
}

void EndingPageLogic::on_resume() {
  lfw().sounds_stop_bgm();
  Jalousie* const j = jalousie();
  if (j != nullptr && j->anim().done()) j->set_open(!j->open());
}

void EndingPageLogic::on_stop() {
  UIComponent::on_stop();
  Jalousie* const j = jalousie();
  if (j != nullptr) j->callbacks.del(&dptr->cb);
}

void EndingPageLogic::on_click(LFWPointerEvent& e) {
  (void)e;
  Jalousie* const j = jalousie();
  if (j != nullptr && j->anim().done()) j->set_open(!j->open());
}

void EndingPageLogic::on_key_down(LFWKeyEvent& e) {
  (void)e;
  Jalousie* const j = jalousie();
  if (j != nullptr && j->anim().done()) j->set_open(!j->open());
}

void EndingPageLogic::on_jalousie_anim_end(Jalousie& j) {
  DPtr& d = *dptr;
  if (j.open()) return;
  UINode* const label = label_node();
  if (label != nullptr) {
    if (d.texts_idx == -1 && d.difficulty() >= static_cast<double>(Difficulty::Difficult)) {
      d.texts_idx += 2;
    } else {
      d.texts_idx += 1;
    }
    if (d.texts_idx >= static_cast<double>(d.texts.size())) {
      UILayer* const layer = node.layer();
      if (layer != nullptr) {
        if (layer->pages().size() > 1) {
          UIPopPageOpts opts;
          opts.until = [](UINode&, double i, const std::vector<UINode*>&) { return i == 0; };
          layer->pop(opts);
        } else {
          UIPushPageOpts opts;
          opts.id = u"main_page";
          opts.has_id = true;
          layer->set(opts);
          lfw().sounds_play_bgm(Value(std::u16string(u"bgm/main.wma.mp3")));
        }
      }
    } else {
      label->set_text(d.texts[static_cast<size_t>(d.texts_idx)]);
    }
  }
  j.set_open(true);
}

}
}
