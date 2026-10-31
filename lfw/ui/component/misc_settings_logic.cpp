#include "lfw/ui/component/misc_settings_logic.h"

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/defines/sync_render_enum.h"
#include "lfw/lfw.h"
#include "lfw/ui/component/arrow_switcher.h"
#include "lfw/ui/component/slider_handle.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/is_class.h"
#include "lfw/utils/math/round_float.h"

namespace lfw {
namespace ui {

namespace {

const std::vector<double> kRenderRateOptions = {
    static_cast<double>(SyncRenderEnum::Half),   static_cast<double>(SyncRenderEnum::Sync),
    static_cast<double>(SyncRenderEnum::FPS_60), static_cast<double>(SyncRenderEnum::FPS_120),
    static_cast<double>(SyncRenderEnum::Unlimited)};

const std::vector<std::u16string> kLangCodes = {u"",      u"zh-hans", u"zh-hant", u"de",
                                                u"es",    u"fr",      u"hu",      u"it",
                                                u"ja",    u"ko",      u"nl",      u"pl",
                                                u"pt",    u"ro",      u"ru"};

double index_of_number(const std::vector<double>& arr, const Value& v) {
  if (std::holds_alternative<NullTag>(v)) return -1.0;
  const double n = to_number(v);
  if (std::isnan(n)) return -1.0;
  for (size_t i = 0; i < arr.size(); ++i) {
    if (arr[i] == n) return static_cast<double>(i);
  }
  return -1.0;
}

Value indexed(const std::vector<double>& arr, double i) {
  if (i != std::floor(i) || i < 0 || i >= static_cast<double>(arr.size())) return Value();
  return Value(arr[static_cast<size_t>(i)]);
}

Value indexed_str(const std::vector<std::u16string>& arr, double i) {
  if (i != std::floor(i) || i < 0 || i >= static_cast<double>(arr.size())) return Value();
  return Value(arr[static_cast<size_t>(i)]);
}

double index_of_str(const std::vector<std::u16string>& arr, const Value& v) {
  if (!std::holds_alternative<std::u16string>(v)) return -1.0;
  const std::u16string& s = std::get<std::u16string>(v);
  for (size_t i = 0; i < arr.size(); ++i) {
    if (arr[i] == s) return static_cast<double>(i);
  }
  return -1.0;
}

void play_preset_cancel(LFW& lfw) {
  lfw.host().sounds_play_preset(Value(u"cancel"), Value(), Value(), Value());
}

void play_preset_ok(LFW& lfw) {
  lfw.host().sounds_play_preset(Value(u"ok"), Value(), Value(), Value());
}

}

struct MiscSettingsLogic::DPtr {
  explicit DPtr(MiscSettingsLogic& owner) : self(owner) {}

  SliderHandle* row_slider(const std::u16string& row_id);
  ArrowSwitcher* row_switcher(const std::u16string& row_id);

  struct SoundsListener : LfwSoundsCallback {
    MiscSettingsLogic* self = nullptr;
    void on_volume_changed(double volume, double prev) override;
    void on_bgm_volume_changed(double volume, double prev) override;
    void on_sound_volume_changed(double volume, double prev) override;
    void on_bgm_muted_changed(bool muted) override;
    void on_sound_muted_changed(bool muted) override;
  };

  MiscSettingsLogic& self;
  SoundsListener sounds_listener;
  bool sounds_added = false;

  std::function<void()> world_remover;
  std::function<void()> lfw_remover;

  SliderHandle* a = nullptr;
  SliderHandle* b = nullptr;
  SliderHandle* c = nullptr;
  SliderHandle* d = nullptr;
  SliderHandle* e = nullptr;
  SliderHandle* f = nullptr;
  SliderHandle* g = nullptr;
  SliderHandle* h = nullptr;
  ArrowSwitcher* i = nullptr;
  SliderHandle* j = nullptr;
};

const ClazzTag* MiscSettingsLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& MiscSettingsLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"MiscSettingsLogic"};
  return tags;
}

MiscSettingsLogic::MiscSettingsLogic(UINode& layout, const std::u16string& f_name, const Value& info)
    : UIComponent(layout, f_name, info), dptr(std::make_unique<DPtr>(*this)) {}

MiscSettingsLogic::~MiscSettingsLogic() = default;

SliderHandle* MiscSettingsLogic::DPtr::row_slider(const std::u16string& row_id) {
  UINode* const row = self.node.search_node(row_id);
  if (row == nullptr) return nullptr;
  UIComponent* const c = row->search_component(SliderHandle::class_tag());
  if (c == nullptr) return nullptr;
  return is_class(c->clazz(), SliderHandle::class_tag()) ? static_cast<SliderHandle*>(c) : nullptr;
}

ArrowSwitcher* MiscSettingsLogic::DPtr::row_switcher(const std::u16string& row_id) {
  UINode* const row = self.node.search_node(row_id);
  if (row == nullptr) return nullptr;
  UIComponent* const c = row->search_component(ArrowSwitcher::class_tag());
  if (c == nullptr) return nullptr;
  return is_class(c->clazz(), ArrowSwitcher::class_tag()) ? static_cast<ArrowSwitcher*>(c)
                                                          : nullptr;
}

SliderHandle* MiscSettingsLogic::bgm_toggle() {
  DPtr& d = *dptr;
  if (d.a == nullptr) d.a = d.row_slider(u"bgm_toggle_row");
  return d.a;
}

SliderHandle* MiscSettingsLogic::bgm_volume() {
  DPtr& d = *dptr;
  if (d.b == nullptr) d.b = d.row_slider(u"bgm_volume_row");
  return d.b;
}

SliderHandle* MiscSettingsLogic::sfx_toggle() {
  DPtr& d = *dptr;
  if (d.c == nullptr) d.c = d.row_slider(u"sfx_toggle_row");
  return d.c;
}

SliderHandle* MiscSettingsLogic::sfx_volume() {
  DPtr& d = *dptr;
  if (d.d == nullptr) d.d = d.row_slider(u"sfx_volume_row");
  return d.d;
}

SliderHandle* MiscSettingsLogic::team_outline() {
  DPtr& d = *dptr;
  if (d.e == nullptr) d.e = d.row_slider(u"team_outline_row");
  return d.e;
}

SliderHandle* MiscSettingsLogic::render_rate() {
  DPtr& d = *dptr;
  if (d.f == nullptr) d.f = d.row_slider(u"render_rate_row");
  return d.f;
}

SliderHandle* MiscSettingsLogic::main_volume() {
  DPtr& d = *dptr;
  if (d.g == nullptr) d.g = d.row_slider(u"main_volume_row");
  return d.g;
}

SliderHandle* MiscSettingsLogic::ups() {
  DPtr& d = *dptr;
  if (d.h == nullptr) d.h = d.row_slider(u"ups_row");
  return d.h;
}

ArrowSwitcher* MiscSettingsLogic::lang_row() {
  DPtr& d = *dptr;
  if (d.i == nullptr) d.i = d.row_switcher(u"language_row");
  return d.i;
}

SliderHandle* MiscSettingsLogic::stats_visible() {
  DPtr& d = *dptr;
  if (d.j == nullptr) d.j = d.row_slider(u"stats_visible_row");
  return d.j;
}

void MiscSettingsLogic::DPtr::SoundsListener::on_volume_changed(double volume, double prev) {
  (void)prev;
  SliderHandle* const p = self != nullptr ? self->main_volume() : nullptr;
  if (p != nullptr) p->set_factor(volume);
}

void MiscSettingsLogic::DPtr::SoundsListener::on_bgm_volume_changed(double volume, double prev) {
  (void)prev;
  SliderHandle* const p = self != nullptr ? self->bgm_volume() : nullptr;
  if (p != nullptr) p->set_factor(volume);
}

void MiscSettingsLogic::DPtr::SoundsListener::on_sound_volume_changed(double volume, double prev) {
  (void)prev;
  SliderHandle* const p = self != nullptr ? self->sfx_volume() : nullptr;
  if (p != nullptr) p->set_factor(volume);
}

void MiscSettingsLogic::DPtr::SoundsListener::on_bgm_muted_changed(bool muted) {
  SliderHandle* const p = self != nullptr ? self->bgm_toggle() : nullptr;
  if (p != nullptr) p->set_value(muted ? 0.0 : 1.0);
}

void MiscSettingsLogic::DPtr::SoundsListener::on_sound_muted_changed(bool muted) {
  SliderHandle* const p = self != nullptr ? self->sfx_toggle() : nullptr;
  if (p != nullptr) p->set_value(muted ? 0.0 : 1.0);
}

void MiscSettingsLogic::on_start() {
  UIComponent::on_start();
  DPtr& d = *dptr;
  d.sounds_listener.self = this;
  if (!d.sounds_added) {
    lfw().sounds_callback_add(&d.sounds_listener);
    d.sounds_added = true;
  }
  if (!d.world_remover) {
    d.world_remover = world().callbacks.on(
        u"on_dataset_change", [this](const WorldCallbacks::Payloads& ps) {
          for (const WorldCallbackArgs& a : ps) {
            if (a.key != u"sync_render") continue;
            SliderHandle* const p = render_rate();
            if (p != nullptr) p->set_value(index_of_number(kRenderRateOptions, a.value));
          }
        });
  }
  if (!d.lfw_remover) {
    d.lfw_remover = lfw().callbacks.on(u"on_broadcast", [this](const LfwCallbacks::Payloads& ps) {
      for (const LfwCallbackArgs& a : ps) {
        SliderHandle* const p = stats_visible();
        if (p == nullptr) continue;
        if (a.text == u"stats_visible:1") p->set_value(1.0);
        else if (a.text == u"stats_visible:0") p->set_value(0.0);
      }
    });
  }
  SliderHandle* const mv = main_volume();
  if (mv != nullptr) {
    mv->on_value_changed([this](double, SliderHandle& s) {
      lfw().sounds_set_volume(s.factor());
      play_preset_cancel(lfw());
    });
  }
  SliderHandle* const bt = bgm_toggle();
  if (bt != nullptr) {
    bt->on_value_changed([this](double v, SliderHandle&) {
      lfw().sounds_set_bgm_muted(!truthy(Value(v)));
      if (!truthy(Value(v))) play_preset_cancel(lfw());
      else play_preset_ok(lfw());
    });
  }
  SliderHandle* const bv = bgm_volume();
  if (bv != nullptr) {
    bv->on_value_changed([this](double, SliderHandle& s) {
      lfw().sounds_set_bgm_volume(s.factor());
      play_preset_cancel(lfw());
    });
  }
  SliderHandle* const st = sfx_toggle();
  if (st != nullptr) {
    st->on_value_changed([this](double v, SliderHandle&) {
      lfw().sounds_set_sound_muted(!truthy(Value(v)));
      if (!truthy(Value(v))) play_preset_cancel(lfw());
      else play_preset_ok(lfw());
    });
  }
  SliderHandle* const sv = sfx_volume();
  if (sv != nullptr) {
    sv->on_value_changed([this](double, SliderHandle& s) {
      lfw().sounds_set_sound_volume(s.factor());
      play_preset_cancel(lfw());
    });
  }
  SliderHandle* const to = team_outline();
  if (to != nullptr) {
    to->on_value_changed([this](double v, SliderHandle&) {
      world().dataset.set(u"outline_enabled", Value(v));
      if (truthy(Value(v))) play_preset_ok(lfw());
      else play_preset_cancel(lfw());
    });
  }
  SliderHandle* const rr = render_rate();
  if (rr != nullptr) {
    rr->on_value_changed([this](double v, SliderHandle&) {
      world().dataset.set(u"sync_render", indexed(kRenderRateOptions, v));
      play_preset_ok(lfw());
    });
  }
  const std::vector<double> ups_arr = {30.0, 60.0};
  std::vector<double> atom_time_arr;
  std::vector<double> double_click_interval_arr;
  std::vector<double> key_hit_duration_arr;
  for (const double v : ups_arr) {
    atom_time_arr.push_back(round_float(60.0 / v));
    double_click_interval_arr.push_back(30.0 * v / 60.0);
    key_hit_duration_arr.push_back(10.0 * v / 60.0);
  }
  SliderHandle* const up = ups();
  if (up != nullptr) {
    up->on_value_changed([this, ups_arr, atom_time_arr, double_click_interval_arr,
                          key_hit_duration_arr](double v, SliderHandle&) {
      world().dataset.set(u"UPS", indexed(ups_arr, v));
      world().dataset.set(u"atom_time", indexed(atom_time_arr, v));
      world().dataset.set(u"wait_offset", Value(0.0));
      world().dataset.set(u"fvy_f", Value(-0.5));
      world().dataset.set(u"double_click_interval", indexed(double_click_interval_arr, v));
      world().dataset.set(u"key_hit_duration", indexed(key_hit_duration_arr, v));
      play_preset_ok(lfw());
    });
  }
  ArrowSwitcher* const lr = lang_row();
  if (lr != nullptr) {
    lr->on_value_changed([this](double v, ArrowSwitcher&) {
      lfw().set_lang(indexed_str(kLangCodes, v));
      play_preset_ok(lfw());
    });
  }
  SliderHandle* const svis = stats_visible();
  if (svis != nullptr) {
    svis->on_value_changed([this](double v, SliderHandle&) {
      std::u16string msg = u"stats_visible_set:";
      msg += truthy(Value(v)) ? u"1" : u"0";
      lfw().broadcast(Value(msg));
      if (truthy(Value(v))) play_preset_ok(lfw());
      else play_preset_cancel(lfw());
    });
  }
}

void MiscSettingsLogic::on_resume() {
  UIComponent::on_resume();
  lfw().broadcast(Value(u"stats_visible_get"));
  SliderHandle* const mv = main_volume();
  if (mv != nullptr) mv->set_factor(lfw().sounds_volume());
  SliderHandle* const bt = bgm_toggle();
  if (bt != nullptr) bt->set_value(lfw().sounds_bgm_muted() ? 0.0 : 1.0);
  SliderHandle* const bv = bgm_volume();
  if (bv != nullptr) bv->set_factor(lfw().sounds_bgm_volume());
  SliderHandle* const sv = sfx_volume();
  if (sv != nullptr) sv->set_factor(lfw().sounds_sound_volume());
  SliderHandle* const st = sfx_toggle();
  if (st != nullptr) st->set_value(lfw().sounds_sound_muted() ? 0.0 : 1.0);
  SliderHandle* const to = team_outline();
  if (to != nullptr) to->set_factor(world().dataset.outline_enabled());
  SliderHandle* const rr = render_rate();
  if (rr != nullptr) rr->set_value(index_of_number(kRenderRateOptions, Value(world().dataset.sync_render())));
  const std::vector<double> ups_arr = {30.0, 60.0};
  const double ups_idx = index_of_number(ups_arr, Value(world().dataset.UPS()));
  SliderHandle* const up = ups();
  if (up != nullptr) up->set_value(ups_idx < 0 ? 1.0 : ups_idx);
  const Value cur_lang = lfw().canonical_lang();
  ArrowSwitcher* const lr = lang_row();
  if (lr != nullptr) lr->set_value(index_of_str(kLangCodes, cur_lang));
}

void MiscSettingsLogic::on_stop() {
  UIComponent::on_stop();
  DPtr& d = *dptr;
  if (d.sounds_added) {
    lfw().sounds_callback_del(&d.sounds_listener);
    d.sounds_added = false;
  }
  if (d.world_remover) {
    d.world_remover();
    d.world_remover = nullptr;
  }
  if (d.lfw_remover) {
    d.lfw_remover();
    d.lfw_remover = nullptr;
  }
}

}
}
