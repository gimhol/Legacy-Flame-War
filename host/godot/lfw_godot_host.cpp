#include "host/godot/lfw_godot_host.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "host/common/zip_file.h"
#include "host/godot/lfw_godot_http.h"
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/value.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

namespace lfw::host {

namespace {

std::u16string u16_or(const Value& v, const std::u16string& fallback) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : fallback;
}

const std::u16string* as_str(const Value& v) { return std::get_if<std::u16string>(&v); }

/** latest.json 的 `version` 是否比本地新（按 `a.b.c` 比） */
bool version_is_newer(const std::u16string& latest, const std::u16string& local) {
  const auto parts = [](const std::u16string& s) {
    std::vector<double> out;
    std::u16string cur;
    for (size_t i = 0; i <= s.size(); ++i) {
      const char16_t c = i < s.size() ? s[i] : u'.';
      if (c == u'.' || c == u'-' || c == u'+') {
        out.push_back(cur.empty() ? 0.0 : to_number(Value(cur)));
        cur.clear();
      } else if (c >= u'0' && c <= u'9') {
        cur.push_back(c);
      }
    }
    return out;
  };
  std::u16string a = latest;
  std::u16string b = local;
  if (!a.empty() && a[0] == u'v') a = a.substr(1);
  if (!b.empty() && b[0] == u'v') b = b.substr(1);
  const std::vector<double> x = parts(a);
  const std::vector<double> y = parts(b);
  const size_t n = std::max(x.size(), y.size());
  for (size_t i = 0; i < n; ++i) {
    const double xv = i < x.size() ? x[i] : 0.0;
    const double yv = i < y.size() ? y[i] : 0.0;
    if (xv != yv) return xv > yv;
  }
  return false;
}

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

LfwGodotHost::~LfwGodotHost() {
  if (_remote == nullptr || _remote->persist()) return;
  // Windows 上文件还开着就删不掉：先把懒读的包关掉再删临时目录
  _remote_zips.clear();
  std::error_code ec;
  std::filesystem::remove_all(_remote->dir(), ec);
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

namespace {

std::string join_values(const std::vector<Value>& args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0) out += " ";
    const std::vector<uint8_t> buf = lfw::encode_utf8(lfw::to_string(args[i]));
    out += std::string(buf.begin(), buf.end());
  }
  return out;
}

}

void LfwGodotHost::warn(const std::vector<Value>& args) {
  if (_console_sink) _console_sink(2, join_values(args));
  HeadlessHost::warn(args);
}

void LfwGodotHost::error(const std::vector<Value>& args) {
  if (_console_sink) _console_sink(3, join_values(args));
  HeadlessHost::error(args);
}

void LfwGodotHost::log(const std::vector<Value>& args) {
  if (_console_sink) _console_sink(1, join_values(args));
  HeadlessHost::log(args);
}

// ---------------------------------------------------------------------------
// 远端数据包（http/https）+ 落盘缓存
//
// 核心（`LFW::load_zip_from_url`）已经写好了整套流程：取 `__info.json` → 查缓存 →
// 断点续传 → md5 校验 → 打开 zip。宿主这边只需要实现 ILfwHost 的取数/缓存钩子。
// ---------------------------------------------------------------------------

namespace {

std::string to_u8(const std::u16string& s) {
  const std::vector<uint8_t> buf = lfw::encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

std::filesystem::path path_of_u16(const std::u16string& s) {
  return std::filesystem::path(to_u8(s));
}

}  // namespace

void LfwGodotHost::remote_log(const std::u16string& text) { log({Value(text)}); }

void LfwGodotHost::enable_remote_cache(const std::filesystem::path& dir, bool persist) {
  _remote = std::make_unique<RemoteCache>(dir, persist);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const bool index_ok = _remote->load();
  remote_log(u"[remote] cache " + from_utf8(dir.generic_string()) + u" (" +
             from_utf8(std::to_string(_remote->count())) + u" packs" +
             (persist ? u"" : u", not persisted") + u")" +
             (index_ok ? std::u16string() : u" [index unreadable, starting fresh]"));
}

std::u16string LfwGodotHost::remote_key(const std::u16string& url,
                                        const std::u16string& md5) const {
  if (!md5.empty()) return md5;
  return from_utf8(md5_hex(to_u8(url)));
}

bool LfwGodotHost::fetch_remote_text(const std::u16string& url, std::u16string& text,
                                     std::u16string& error) {
  std::vector<uint8_t> bytes;
  HttpResult result;
  if (!http_get(url, {}, bytes, result, error)) return false;
  text = lfw::decode_utf8(bytes);
  return true;
}

bool LfwGodotHost::import_as_json(const std::vector<std::u16string>& urls, Value& data,
                                  Value& hit, std::u16string& error) {
  if (_remote == nullptr) return HeadlessHost::import_as_json(urls, data, hit, error);
  for (const std::u16string& url : urls) {
    if (!is_http_url(url)) {
      Value local_data;
      Value local_hit;
      std::u16string local_error;
      if (HeadlessHost::import_as_json({url}, local_data, local_hit, local_error)) {
        data = local_data;
        hit = local_hit;
        return true;
      }
      error = local_error;
      continue;
    }
    std::u16string text;
    if (!fetch_remote_text(url, text, error)) continue;
    const Json5Result parsed = json5_parse(text);
    if (parsed.ok) {
      data = parsed.value;
      hit = Value(url);
      return true;
    }
    const std::optional<Value> strict = json_parse(text);
    if (strict.has_value()) {
      data = *strict;
      hit = Value(url);
      return true;
    }
    error = u"[remote] json parse failed: " + url;
  }
  if (error.empty()) {
    error = u"[remote] import_as_json not found: " +
            (urls.empty() ? std::u16string(u"(no url)") : urls.front());
  }
  return false;
}

bool LfwGodotHost::import_as_text(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                                  std::u16string& error) {
  if (_remote == nullptr) return HeadlessHost::import_as_text(urls, data, hit, error);
  for (const std::u16string& url : urls) {
    if (!is_http_url(url)) {
      Value local_data;
      Value local_hit;
      std::u16string local_error;
      if (HeadlessHost::import_as_text({url}, local_data, local_hit, local_error)) {
        data = local_data;
        hit = local_hit;
        return true;
      }
      error = local_error;
      continue;
    }
    std::u16string text;
    if (!fetch_remote_text(url, text, error)) continue;
    data = Value(text);
    hit = Value(url);
    return true;
  }
  if (error.empty()) {
    error = u"[remote] import_as_text not found: " +
            (urls.empty() ? std::u16string(u"(no url)") : urls.front());
  }
  return false;
}

bool LfwGodotHost::import_as_array_buffer(const std::vector<std::u16string>& urls, Value& data,
                                          Value& hit, std::u16string& error) {
  if (_remote == nullptr) return HeadlessHost::import_as_array_buffer(urls, data, hit, error);
  for (const std::u16string& url : urls) {
    if (!is_http_url(url)) {
      Value local_data;
      Value local_hit;
      std::u16string local_error;
      if (HeadlessHost::import_as_array_buffer({url}, local_data, local_hit, local_error)) {
        data = local_data;
        hit = local_hit;
        return true;
      }
      error = local_error;
      continue;
    }
    std::vector<uint8_t> bytes;
    HttpResult result;
    if (!http_get(url, {}, bytes, result, error)) continue;
    auto arr = std::make_shared<Array>();
    for (const uint8_t byte : bytes) arr->push_back(Value(static_cast<double>(byte)));
    data = Value(std::move(arr));
    hit = Value(url);
    return true;
  }
  if (error.empty()) {
    error = u"[remote] import_as_array_buffer not found: " +
            (urls.empty() ? std::u16string(u"(no url)") : urls.front());
  }
  return false;
}

Value LfwGodotHost::zip_get_stored(const std::u16string& zip_url, const std::u16string& md5) {
  if (_remote == nullptr) return HeadlessHost::zip_get_stored(zip_url, md5);
  // 断点续传在 zip_download 内部处理；这里只回答「缓存里已经有整包了吗」——
  // 有的话下面的 zip_cache_get 会命中，没有就让核心去下载。
  return Value();
}

Value LfwGodotHost::zip_cache_get(const std::u16string& name) {
  if (_remote == nullptr) return HeadlessHost::zip_cache_get(name);
  const RemoteCache::Entry* const entry = _remote->find(name);
  if (entry == nullptr) return Value();
  const std::filesystem::path path = _remote->file_of(*entry);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    _remote->erase(name);
    _remote->save();
    return Value();
  }
  auto info = std::make_shared<Object>();
  info->set(u"name", Value(name));
  info->set(u"md5", Value(entry->md5));
  info->set(u"version", Value(entry->version));
  info->set(u"size", Value(static_cast<double>(entry->size)));
  info->set(u"blob", Value(u"file:" + from_utf8(path.generic_string())));
  remote_log(u"[remote] cache hit: " + from_utf8(path.filename().generic_string()));
  return Value(info);
}

void LfwGodotHost::zip_cache_put(const Value& entry) {
  if (_remote == nullptr) {
    HeadlessHost::zip_cache_put(entry);
    return;
  }
  const std::u16string name = u16_or(lfw::field_or(entry, u"name"), std::u16string());
  const std::u16string blob = u16_or(lfw::field_or(entry, u"blob"), std::u16string());
  if (name.empty() || blob.rfind(u"file:", 0) != 0) {
    HeadlessHost::zip_cache_put(entry);
    return;
  }
  const std::filesystem::path path = path_of_u16(blob.substr(5));
  RemoteCache::Entry record;
  record.md5 = name;
  record.version = u16_or(lfw::field_or(entry, u"version"), std::u16string());
  record.file = from_utf8(path.filename().generic_string());
  record.time = now();
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (!ec) record.size = static_cast<long long>(size);
  _remote->put(record);
  _remote->save();
}

void LfwGodotHost::zip_cache_del(const std::u16string& name, const std::u16string& version) {
  if (_remote == nullptr) {
    HeadlessHost::zip_cache_del(name, version);
    return;
  }
  if (const RemoteCache::Entry* const entry = _remote->find(name)) {
    const std::filesystem::path path = _remote->file_of(*entry);
    if (_remote->owns(path)) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
    _remote->erase(name);
    _remote->save();
  }
  HeadlessHost::zip_cache_del(name, version);
}

bool LfwGodotHost::zip_read_blob(const std::u16string& name, const Value& blob, const Value& md5,
                                 IZip*& out, std::u16string& error) {
  const std::u16string* const token = as_str(blob);
  if (_remote == nullptr || token == nullptr || token->rfind(u"file:", 0) != 0) {
    return HeadlessHost::zip_read_blob(name, blob, md5, out, error);
  }
  const std::filesystem::path path = path_of_u16(token->substr(5));
  const std::u16string* const want = as_str(md5);
  const std::optional<std::u16string> expect =
      want != nullptr && !want->empty() ? std::optional<std::u16string>(*want) : std::nullopt;
  std::unique_ptr<ZipFile> zip;
  if (!ZipFile::open_cached(name, path, expect, zip, error)) return false;
  _remote_zips.push_back(std::move(zip));
  out = _remote_zips.back().get();
  return true;
}

bool LfwGodotHost::zip_download(const std::u16string& zip_url, const Value& md5, LFW& lfw,
                                DownloadedZip& out, std::u16string& error) {
  if (_remote == nullptr || !is_http_url(zip_url)) {
    return HeadlessHost::zip_download(zip_url, md5, lfw, out, error);
  }
  const std::u16string want = u16_or(md5, std::u16string());
  const std::u16string key = remote_key(zip_url, want);
  const std::filesystem::path target = _remote->zip_path(key);
  const std::filesystem::path part = _remote->part_path(key);
  std::error_code ec;

  // 1) 已经有完整的包：校验 md5 后直接用
  if (std::filesystem::is_regular_file(target, ec)) {
    const std::string actual = md5_hex_file(target);
    if (!actual.empty() && (want.empty() || from_utf8(actual) == want)) {
      remote_log(u"[remote] cache hit: " + from_utf8(target.filename().generic_string()));
      out.stored = false;
      out.blob = Value(u"file:" + from_utf8(target.generic_string()));
      out.md5 = Value(from_utf8(actual));
      return true;
    }
    std::filesystem::remove(target, ec);
  }

  // 2) 断点续传下载
  const bool resuming = std::filesystem::is_regular_file(part, ec);
  const auto part_size = std::filesystem::file_size(part, ec);
  if (resuming && !ec && part_size > 0) {
    remote_log(u"[remote] resume at " + from_utf8(std::to_string(part_size)) + u" bytes: " + zip_url);
  } else {
    remote_log(u"[remote] download: " + zip_url);
  }

  HttpResult result;
  int last_bucket = -1;
  const HttpProgressFn progress = [this, &lfw, &zip_url, &last_bucket](long long received,
                                                                      long long total) {
    const double pct = total > 0
                           ? static_cast<double>(received) * 100.0 / static_cast<double>(total)
                           : 0.0;
    // 每 10% 打一行：headless 也能看出远端下载的进度
    const int bucket = static_cast<int>(pct / 10.0);
    if (bucket != last_bucket) {
      last_bucket = bucket;
      std::string line = "[remote] " + std::to_string(bucket * 10) + "% " +
                         std::to_string(received) + "/" +
                         (total > 0 ? std::to_string(total) : std::string("?")) + " ";
      const std::vector<uint8_t> raw = lfw::encode_utf8(zip_url);
      line += std::string(raw.begin(), raw.end());
      log({Value(lfw::decode_utf8(std::vector<uint8_t>(line.begin(), line.end())))});
    }
    lfw.on_loading_file(zip_url, pct, total > 0 ? static_cast<double>(total) : 0.0);
  };

  // 下载 + 校验；`resume` = 接着 `.part` 写。校验失败时外层会清掉半截再整包重来一次。
  const auto attempt = [&](bool resume) -> bool {
    error.clear();
    std::error_code attempt_ec;
    if (!http_download(zip_url, part, resume, {}, result, error, progress)) return false;
    const std::string actual = md5_hex_file(part);
    if (actual.empty()) {
      error = u"[remote] cannot hash downloaded file: " + zip_url;
      return false;
    }
    if (!want.empty() && from_utf8(actual) != want) {
      error = u"[remote] md5 mismatch (want " + want + u", got " + from_utf8(actual) +
              u"): " + zip_url;
      return false;
    }
    (void)attempt_ec;
    return true;
  };

  if (!attempt(true)) {
    // 半截续传可能根上就是坏的（上次下到一半的内容不对）：清掉重下一次
    const std::filesystem::path failed_part = part;
    std::filesystem::remove(failed_part, ec);
    remote_log(u"[remote] retry from scratch: " + zip_url);
    if (!attempt(false)) {
      std::filesystem::remove(failed_part, ec);
      return false;
    }
  }
  const std::string actual = md5_hex_file(part);

  // 4) 落地 + 记索引
  std::filesystem::remove(target, ec);
  std::filesystem::rename(part, target, ec);
  if (ec) {
    error = u"[remote] cannot move into cache: " + from_utf8(target.generic_string());
    return false;
  }
  RemoteCache::Entry record;
  record.md5 = key;
  record.file = from_utf8(target.filename().generic_string());
  record.time = now();
  const auto size = std::filesystem::file_size(target, ec);
  if (!ec) record.size = static_cast<long long>(size);
  _remote->put(record);
  _remote->save();
  remote_log(u"[remote] cached: " + record.file + u" (" + from_utf8(std::to_string(record.size)) +
             u" bytes)");

  out.stored = false;
  out.blob = Value(u"file:" + from_utf8(target.generic_string()));
  out.md5 = Value(from_utf8(actual));
  return true;
}

std::filesystem::path LfwGodotHost::remote_version_cache_file() const {
  if (_remote == nullptr) return {};
  return _remote->dir() / "version.json";
}

void LfwGodotHost::check_remote_version(const std::u16string& url) {
  if (_remote == nullptr || !is_http_url(url)) return;
  constexpr double kTtlSec = 6.0 * 3600.0;
  const std::filesystem::path file = remote_version_cache_file();

  Value data;
  bool have = false;
  {
    std::error_code ec;
    if (std::filesystem::is_regular_file(file, ec)) {
      std::ifstream in(file, std::ios::binary);
      if (in) {
        std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::u16string text = lfw::decode_utf8(std::vector<uint8_t>(raw.begin(), raw.end()));
        const Json5Result parsed = json5_parse(text);
        if (parsed.ok) {
          const std::u16string cached_url = u16_or(lfw::field_or(parsed.value, u"url"), u"");
          const Value time_value = lfw::field_or(parsed.value, u"time");
          const double* const time = std::get_if<double>(&time_value);
          if (cached_url == url && time != nullptr && (now() - *time) < kTtlSec) {
            data = lfw::field_or(parsed.value, u"data");
            have = truthy(data);
          }
        }
      }
    }
  }

  if (!have) {
    std::u16string text;
    std::u16string error;
    if (!fetch_remote_text(url, text, error)) {
      warn({Value(u"[remote] version check failed: "), Value(error)});
      return;
    }
    const Json5Result parsed = json5_parse(text);
    const std::optional<Value> strict = json_parse(text);
    if (!parsed.ok && !strict.has_value()) {
      warn({Value(u"[remote] version check: bad json: "), Value(url)});
      return;
    }
    data = parsed.ok ? parsed.value : *strict;

    auto index = std::make_shared<Object>();
    index->set(u"url", Value(url));
    index->set(u"time", Value(now()));
    index->set(u"data", data);
    const std::optional<std::u16string> dump = json_stringify(Value(index));
    if (dump.has_value()) {
      std::ofstream out(file, std::ios::binary | std::ios::trunc);
      if (out) {
        const std::vector<uint8_t> raw = lfw::encode_utf8(*dump);
        out.write(reinterpret_cast<const char*>(raw.data()),
                  static_cast<std::streamsize>(raw.size()));
      }
    }
  }

  const std::u16string latest = u16_or(lfw::field_or(data, u"version"), u"");
  if (latest.empty()) {
    remote_log(u"[remote] version check: no 'version' field in " + url);
    return;
  }
  const std::u16string local = LFW::VERSION_NAME();
  const bool newer = version_is_newer(latest, local);
  remote_log(u"[remote] version check: local=" + local + u" latest=" + latest +
             (newer ? u" (update available)" : u" (up to date)"));
}

}
