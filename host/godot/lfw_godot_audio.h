#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/audio_effect_panner.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>

namespace lfw::host {

class LfwGodotHost;

/**
 * Godot 侧的音频输出：`kSlots` 个 SFX 槽（每槽一条总线 + Panner）与 1 个 BGM 播放器。
 *
 * 解码用的字节由宿主注入的 `ReadFn` 提供（控制台/数据包都在那一侧），
 * 统计与"播完"通知走 `LfwGodotHost`，BGM 播完后的续播决定交给 `finished` 钩子
 * ⇒ 这里不碰 `Node` 树以外的东西，也不认识 `World`。
 */
class GodotAudio {
 public:
  using ReadFn = std::function<bool(const std::string& path, std::string& bytes)>;

  /** 建播放器并把自己的 sink 装进 `host`（可重复调用，第二次直接返回） */
  void install(godot::Node& owner, LfwGodotHost& host);
  bool installed() const { return _owner != nullptr; }

  void set_reader(ReadFn fn) { _read = std::move(fn); }
  /** BGM 自然播完时回调（随机模式下宿主用来排下一首） */
  void set_finished_hook(std::function<void()> fn) { _finished = std::move(fn); }

  void play(const std::string& id, const std::string& path, double gain, double pan);
  void set_gain(const std::string& id, double gain, double pan);
  void preload(const std::string& path);
  void play_bgm(const std::string& path, bool loop, double gain);
  void set_bgm_gain(double gain);
  void stop_bgm();
  void stop_all();

  /** 每帧：回收播完的槽、处理 BGM 续播；`log` 为真时把变化的统计打到 stdout */
  void poll(bool log);
  /** 一行统计（`preload=… decode=… bgm_playing=…`），没有宿主时是空串 */
  std::string report() const;
  bool bgm_playing() const;

 private:
  /** 取（并缓存）一条流；返回空串表示读不到 / 解不了 */
  std::string stream_key(const std::string& path);

  static constexpr int kSlots = 24;

  godot::Node* _owner = nullptr;
  LfwGodotHost* _host = nullptr;
  ReadFn _read;
  std::function<void()> _finished;

  std::vector<godot::AudioStreamPlayer*> _slots;
  std::vector<godot::Ref<godot::AudioEffectPanner>> _pans;
  std::vector<std::string> _ids;
  int _next = 0;
  godot::AudioStreamPlayer* _bgm = nullptr;
  std::map<std::string, godot::Ref<godot::AudioStream>> _cache;
  long long _decoded = 0;
  long long _failed = 0;
  std::string _last_path;
  bool _bgm_was_playing = false;
  std::string _report;
};

}
