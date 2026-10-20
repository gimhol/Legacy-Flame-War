#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "host/godot/lfw_godot_renderer.h"
#include "host/godot/lfw_godot_ui_renderer.h"
#include "host/headless_host.h"
#include "lfw/helper/randoming.h"

namespace lfw {
class World;
}

namespace lfw::host {

class LfwGodotHost : public HeadlessHost {
 public:
  LfwGodotHost(std::vector<std::filesystem::path> roots, bool dev);

  void set_step_ms(double step_ms) { _step_ms = step_ms; }
  double step_ms() const { return _step_ms; }

  void accumulate(double delta_seconds) { _accumulator_ms += delta_seconds * 1000.0; }

  bool can_step() const { return _accumulator_ms >= _step_ms; }

  void push_frame(World& world);

  double residual_ms() const { return _accumulator_ms; }
  long long steps() const { return _steps; }

  IWorldRenderer* create_world_renderer(LFW& lfw) override {
    _renderer = std::make_unique<GodotWorldRenderer>(lfw, *this);
    return _renderer.get();
  }
  GodotWorldRenderer* world_renderer() const { return _renderer.get(); }

  ui::IUINodeRenderer* create_ui_node_renderer(ui::UINode& node) override {
    static_cast<void>(node);
    if (_renderer == nullptr) return nullptr;
    return new GodotUINodeRenderer(*_renderer);
  }

  using MeasureTextFn = std::function<Value(const Value& text, const Value& style)>;
  void set_measure_text(MeasureTextFn fn) { _measure_text = std::move(fn); }
  Value measure_text(const Value& text, const Value& style) override;

  bool ui_image_load(const std::u16string& img_key, const Value& path, const Value& ops, Value& out,
                     std::u16string& error) override;

  struct AudioSink {
    std::function<void(const std::string& path)> preload;
    std::function<void(const std::string& id, const std::string& path, double gain, double pan)> play;
    std::function<void(const std::string& id, double gain, double pan)> set_gain;
    std::function<void()> stop_all;
    std::function<void(const std::string& path, bool loop, double gain)> play_bgm;
    std::function<void(double gain)> set_bgm_gain;
    std::function<void()> stop_bgm;
  };
  void set_audio_sink(AudioSink sink) { _audio = std::move(sink); }

  void on_audio_finished(const std::string& id);

  long long audio_played() const { return _audio_played; }
  long long audio_bgm_played() const { return _audio_bgm_played; }
  long long audio_loaded() const { return _audio_loaded; }
  int audio_active() const { return static_cast<int>(_playings.size()); }
  const std::string& audio_last() const { return _audio_last; }
  const std::string& audio_bgm_path() const { return _bgm_path; }
  bool audio_bgm_playing() const { return _bgm_playing; }
  bool audio_bgm_random() const { return _bgm_random; }
  double audio_bgm_gain() const { return bgm_gain(); }

  void sounds_init(LFW& lfw) override;
  std::function<void()> sounds_play_bgm(const Value& music) override;
  void sounds_stop_bgm() override;
  std::u16string sounds_bgm() override;
  bool sounds_is_random() override;
  void sounds_play(const Value& path, const Value& x, const Value& y, const Value& z) override;
  void sounds_play_preset(const Value& name, const Value& x, const Value& y,
                          const Value& z) override;
  void sounds_play_with_load(const Value& path) override;
  void sounds_load(const Value& name, const Value& src) override;
  void sounds_set_volume(double v) override;
  void sounds_set_bgm_volume(double v) override;
  void sounds_set_sound_volume(double v) override;
  void sounds_set_bgm_muted(bool v) override;
  void sounds_set_sound_muted(bool v) override;
  void sounds_dispose() override;

 private:
  struct Playing {
    Value x;
  };

  void sound_lr(const Value& x, double& l_vol, double& r_vol) const;
  double bgm_gain() const;
  void apply_sound_volume();
  void apply_bgm_volume();
  void emit_volume_changed(double v, double prev);
  void emit_bgm_volume_changed(double v, double prev);
  void emit_sound_volume_changed(double v, double prev);
  void emit_bgm_muted_changed(bool muted);
  void emit_sound_muted_changed(bool muted);
  void emit_bgm_changed(const std::u16string& bgm, const std::u16string& prev);

  double _step_ms = 1000.0 / 60.0;
  double _accumulator_ms = 0.0;
  long long _steps = 0;
  std::unique_ptr<GodotWorldRenderer> _renderer;
  MeasureTextFn _measure_text;
  LFW* _lfw = nullptr;

  AudioSink _audio;
  std::map<std::string, Playing> _playings;
  long long _audio_id = 0;
  long long _audio_played = 0;
  long long _audio_bgm_played = 0;
  long long _audio_loaded = 0;
  std::string _audio_last;
  std::string _bgm_path;
  bool _bgm_random = false;
  bool _bgm_playing = false;
  long long _bgm_req_id = 0;
  std::unique_ptr<Randoming> _bgms;
};

}
