#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "host/godot/lfw_godot_remote_cache.h"
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
  /** `--no-remote-cache` 用的临时缓存目录随实例销毁一起删掉 */
  ~LfwGodotHost() override;

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

  /** 游戏内控制台要看宿主消息：kind 1 = log，2 = warn，3 = error */
  using ConsoleSink = std::function<void(int kind, const std::string& text)>;
  void set_console_sink(ConsoleSink fn) { _console_sink = std::move(fn); }

  void warn(const std::vector<Value>& args) override;
  void error(const std::vector<Value>& args) override;
  void log(const std::vector<Value>& args) override;
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

  // ---- 远端数据包（http/https）+ 落盘缓存 ----

  /**
   * 打开远端缓存：`dir` 一般是 Godot 的 `user://cache/data`。
   * 不调用 = 保持 headless 的「只认本地文件」行为（headless 验收用）。
   */
  void enable_remote_cache(const std::filesystem::path& dir, bool persist = true);
  bool remote_enabled() const { return _remote != nullptr; }
  /** 缓存里已落盘的包数量（诊断 / 日志用） */
  std::size_t remote_cached_count() const {
    return _remote != nullptr ? _remote->count() : 0;
  }
  /** 远端 `latest.json` 版本检查（带 TTL 缓存，失败只记日志） */
  void check_remote_version(const std::u16string& url);

  bool import_as_json(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                      std::u16string& error) override;
  bool import_as_text(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                      std::u16string& error) override;
  bool import_as_array_buffer(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                              std::u16string& error) override;
  bool zip_download(const std::u16string& zip_url, const Value& md5, LFW& lfw, DownloadedZip& out,
                    std::u16string& error) override;
  Value zip_get_stored(const std::u16string& zip_url, const std::u16string& md5) override;
  Value zip_cache_get(const std::u16string& name) override;
  void zip_cache_put(const Value& entry) override;
  void zip_cache_del(const std::u16string& name, const std::u16string& version) override;
  bool zip_read_blob(const std::u16string& name, const Value& blob, const Value& md5, IZip*& out,
                     std::u16string& error) override;

 private:
  /** 远端 url → 缓存键（有 md5 用 md5，没有就用 url 的 md5） */
  std::u16string remote_key(const std::u16string& url, const std::u16string& md5) const;
  bool fetch_remote_text(const std::u16string& url, std::u16string& text, std::u16string& error);
  void remote_log(const std::u16string& text);
  std::filesystem::path remote_version_cache_file() const;

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
  std::unique_ptr<RemoteCache> _remote;
  std::vector<std::unique_ptr<ZipFile>> _remote_zips;
  MeasureTextFn _measure_text;
  LFW* _lfw = nullptr;
  ConsoleSink _console_sink;

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
