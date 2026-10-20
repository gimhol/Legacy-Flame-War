#include "host/godot/lfw_godot_host.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

namespace lfw::host {

namespace {

std::string str_or(const Value& v, const std::string& fallback) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  if (s == nullptr) return fallback;
  const std::vector<uint8_t> buf = lfw::encode_utf8(*s);
  return std::string(buf.begin(), buf.end());
}

double num_or(const Value& holder, const char16_t* key, double fallback) {
  const Value v = lfw::field_or(holder, key);
  const double* const d = std::get_if<double>(&v);
  return d != nullptr ? *d : fallback;
}

std::u16string from_utf8(const std::string& s) {
  const std::vector<uint8_t> buf(s.begin(), s.end());
  return lfw::decode_utf8(buf);
}

}

LfwGodotHost::LfwGodotHost(std::vector<std::filesystem::path> roots, bool dev)
    : HeadlessHost(std::move(roots), dev) {
  _volume = 0.3;
  _bgm_volume = 0.5;
  _sound_volume = 1.0;
}

void LfwGodotHost::push_frame(World& world) {
  _accumulator_ms -= _step_ms;
  clock().advance(_step_ms);
  world.update_once(_step_ms);
  ++_steps;
}

Value LfwGodotHost::measure_text(const Value& text, const Value& style) {
  if (_measure_text) return _measure_text(text, style);
  return HeadlessHost::measure_text(text, style);
}

bool LfwGodotHost::ui_image_load(const std::u16string& img_key, const Value& path, const Value& ops,
                                 Value& out, std::u16string& error) {
  if (!HeadlessHost::ui_image_load(img_key, path, ops, out, error)) return false;
  Object* const info = as_object(out);
  if (info == nullptr) return true;

  double crop_w = 0, crop_h = 0;
  const Array* const op_list = as_array(ops);
  for (size_t i = 0; op_list != nullptr && i < op_list->size(); ++i) {
    const Value op = op_list->at(i);
    if (str_or(lfw::field_or(op, u"type"), "") != "crop") continue;
    crop_w = num_or(op, u"dw", 0.0);
    crop_h = num_or(op, u"dh", 0.0);
  }

  double scale = 1.0, w = 0.0, h = 0.0;
  if (_renderer != nullptr) {
    const GodotWorldRenderer::Resolved& res = _renderer->probe_path(str_or(path, ""));
    scale = res.scale > 0.0 ? res.scale : 1.0;
    w = res.img_w * scale;
    h = res.img_h * scale;
    if (crop_w > 0.0) w = crop_w * scale;
    if (crop_h > 0.0) h = crop_h * scale;
  }
  info->set(u"w", Value(w));
  info->set(u"h", Value(h));
  info->set(u"scale", Value(scale));
  return true;
}

namespace {

constexpr double kDefaultVolume = 0.3;
constexpr double kDefaultBgmVolume = 0.5;
constexpr double kDefaultSoundVolume = 1.0;

constexpr double kClassicScreenWidth = 794.0;

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

}

void LfwGodotHost::sound_lr(const Value& x, double& l_vol, double& r_vol) const {
  const double scale = _lfw != nullptr ? _lfw->world().transform.scale_x() : 1.0;
  const double safe_scale = scale > 0.0 ? scale : 1.0;
  const double full_w = kClassicScreenWidth / safe_scale;
  const double half_w = full_w * 0.5;
  const double viewer_x = _lfw != nullptr ? _lfw->world().camera().position.x + half_w : half_w;
  const double* const xd = std::get_if<double>(&x);
  const double sound_x = xd != nullptr ? *xd : viewer_x;
  const double playings = static_cast<double>(_playings.size()) + 1.0;
  const double attenuation = 1.0 / std::sqrt(playings);
  const double base_vol = _volume * _sound_volume;
  const double final_base = _sound_muted ? 0.0 : base_vol * attenuation;
  const double dx = sound_x - viewer_x;
  const double l_weight = std::max(0.0, 1.0 - std::abs(dx + half_w) / full_w);
  const double r_weight = std::max(0.0, 1.0 - std::abs(dx - half_w) / full_w);
  l_vol = final_base * l_weight * safe_scale;
  r_vol = final_base * r_weight * safe_scale;
  if (!std::isfinite(l_vol) || std::isnan(l_vol)) l_vol = 0.0;
  if (!std::isfinite(r_vol) || std::isnan(r_vol)) r_vol = 0.0;
}

double LfwGodotHost::bgm_gain() const {
  if (_bgm_muted) return 0.0;
  return clamp01(_volume * _bgm_volume);
}

void LfwGodotHost::apply_sound_volume() {
  if (!_audio.set_gain) return;
  for (const auto& kv : _playings) {
    double l = 0.0, r = 0.0;
    sound_lr(kv.second.x, l, r);
    const double sum = l + r;
    _audio.set_gain(kv.first, sum * 0.5, sum > 0.0 ? (r - l) / sum : 0.0);
  }
}

void LfwGodotHost::apply_bgm_volume() {
  if (_bgm_path.empty()) return;
  const double gain = bgm_gain();
  if (!_bgm_playing) {
    if (gain > 0.0 && _audio.play_bgm) {
      _bgm_playing = true;
      _audio.play_bgm(_bgm_path, !_bgm_random, gain);
    }
    return;
  }
  if (_audio.set_bgm_gain) _audio.set_bgm_gain(gain);
}

void LfwGodotHost::on_audio_finished(const std::string& id) { _playings.erase(id); }

std::function<void()> LfwGodotHost::sounds_play_bgm(const Value& music) {
  const std::string name = str_or(music, "");
  if (name.empty()) {
    sounds_stop_bgm();
    return []() {};
  }
  const bool want_random = name == "?";
  if (!want_random && name == _bgm_path) return []() {};
  std::string real = name;
  if (want_random && _lfw != nullptr) {
    if (_bgms == nullptr) {
      _bgms = std::make_unique<Randoming>(u"bgm_randoming", std::vector<Value>{}, &_lfw->mt_ref());
    }
    std::vector<Value> list;
    list.reserve(_lfw->bgms.size());
    for (const std::u16string& one : _lfw->bgms) list.push_back(Value(one));
    _bgms->set_src(std::move(list));
    const Value picked = _bgms->get();
    real = str_or(picked, "");
  }
  const std::u16string prev = from_utf8(_bgm_path);
  if (real.empty()) return []() {};
  ++_audio_bgm_played;
  _bgm_path.clear();
  _bgm_playing = false;
  if (_audio.stop_bgm) _audio.stop_bgm();
  const long long req_id = ++_bgm_req_id;
  _bgm_path = real;
  _bgm_random = want_random;
  apply_bgm_volume();
  emit_bgm_changed(from_utf8(real), prev);
  return [this, req_id]() {
    if (req_id != _bgm_req_id) return;
    sounds_stop_bgm();
  };
}

std::u16string LfwGodotHost::sounds_bgm() { return from_utf8(_bgm_path); }

bool LfwGodotHost::sounds_is_random() { return _bgm_random; }

void LfwGodotHost::sounds_stop_bgm() {
  if (_bgm_path.empty()) return;
  const std::u16string prev = from_utf8(_bgm_path);
  _bgm_path.clear();
  _bgm_playing = false;
  if (_audio.stop_bgm) _audio.stop_bgm();
  emit_bgm_changed(std::u16string(), prev);
}

void LfwGodotHost::sounds_play(const Value& path, const Value& x, const Value& y, const Value& z) {
  (void)y;
  (void)z;
  const std::string key = str_or(path, "");
  if (key.empty()) return;
  ++_audio_played;
  _audio_last = key;
  if (!_audio.play) return;
  double l = 0.0, r = 0.0;
  sound_lr(x, l, r);
  const double sum = l + r;
  const std::string id = std::to_string(++_audio_id);
  _playings.emplace(id, Playing{x});
  _audio.play(id, key, sum * 0.5, sum > 0.0 ? (r - l) / sum : 0.0);
}

void LfwGodotHost::sounds_play_preset(const Value& name, const Value& x, const Value& y,
                                      const Value& z) {
  const std::string t = str_or(name, "");
  if (t == "cancel" || t == "end" || t == "join" || t == "ok" || t == "pass") {
    sounds_play(Value(from_utf8("data/m_" + t + ".wav.mp3")), x, y, z);
    return;
  }
  sounds_play(name, x, y, z);
}

void LfwGodotHost::sounds_play_with_load(const Value& path) {
  sounds_play(path, Value(), Value(), Value());
}

void LfwGodotHost::sounds_load(const Value& name, const Value& src) {
  (void)name;
  const std::string key = str_or(src, "");
  if (key.empty()) return;
  ++_audio_loaded;
  _audio_last = key;
  if (_audio.preload) _audio.preload(key);
}

void LfwGodotHost::sounds_init(LFW& lfw) { _lfw = &lfw; }

void LfwGodotHost::emit_volume_changed(double v, double prev) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_volume_changed(v, prev);
  }
}

void LfwGodotHost::emit_bgm_volume_changed(double v, double prev) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_bgm_volume_changed(v, prev);
  }
}

void LfwGodotHost::emit_sound_volume_changed(double v, double prev) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_sound_volume_changed(v, prev);
  }
}

void LfwGodotHost::emit_bgm_muted_changed(bool muted) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_bgm_muted_changed(muted);
  }
}

void LfwGodotHost::emit_sound_muted_changed(bool muted) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_sound_muted_changed(muted);
  }
}

void LfwGodotHost::emit_bgm_changed(const std::u16string& bgm, const std::u16string& prev) {
  const std::vector<LfwSoundsCallback*> snapshot = _sd_callbacks;
  for (LfwSoundsCallback* const cb : snapshot) {
    if (cb != nullptr) cb->on_bgm_changed(bgm, prev);
  }
}

void LfwGodotHost::sounds_set_volume(double v) {
  v = clamp01(v);
  const double prev = _volume;
  if (v == prev) return;
  _volume = v;
  apply_bgm_volume();
  apply_sound_volume();
  emit_volume_changed(v, prev);
}

void LfwGodotHost::sounds_set_bgm_volume(double v) {
  v = clamp01(v);
  const double prev = _bgm_volume;
  if (v == prev) return;
  _bgm_volume = v;
  apply_bgm_volume();
  emit_bgm_volume_changed(v, prev);
}

void LfwGodotHost::sounds_set_sound_volume(double v) {
  v = clamp01(v);
  const double prev = _sound_volume;
  if (v == prev) return;
  _sound_volume = v;
  apply_sound_volume();
  emit_sound_volume_changed(v, prev);
}

void LfwGodotHost::sounds_set_bgm_muted(bool v) {
  if (v == _bgm_muted) return;
  _bgm_muted = v;
  apply_bgm_volume();
  emit_bgm_muted_changed(v);
}

void LfwGodotHost::sounds_set_sound_muted(bool v) {
  if (v == _sound_muted) return;
  _sound_muted = v;
  apply_sound_volume();
  emit_sound_muted_changed(v);
}

void LfwGodotHost::sounds_dispose() {
  sounds_stop_bgm();
  if (_audio.stop_all) _audio.stop_all();
  _playings.clear();
  HeadlessHost::sounds_dispose();
}

}
