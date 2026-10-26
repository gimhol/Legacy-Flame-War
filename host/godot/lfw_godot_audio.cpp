#include "host/godot/lfw_godot_audio.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "host/godot/lfw_godot_host.h"
#include "host/godot/lfw_godot_renderer.h"

namespace lfw::host {

namespace {

bool bytes_start_with(const std::string& b, const char* magic, std::size_t n) {
  return b.size() >= n && std::memcmp(b.data(), magic, n) == 0;
}

bool looks_like_mp3(const std::string& b) {
  if (bytes_start_with(b, "ID3", 3)) return true;
  return b.size() > 1 && static_cast<unsigned char>(b[0]) == 0xFF &&
         (static_cast<unsigned char>(b[1]) & 0xE0) == 0xE0;
}

godot::PackedByteArray to_packed(const std::string& bytes) {
  godot::PackedByteArray out;
  if (bytes.empty()) return out;
  out.resize(static_cast<int64_t>(bytes.size()));
  std::memcpy(out.ptrw(), bytes.data(), bytes.size());
  return out;
}

void set_stream_loop(const godot::Ref<godot::AudioStream>& stream, bool loop) {
  if (stream.is_null()) return;
  if (godot::AudioStreamMP3* const mp3 = godot::Object::cast_to<godot::AudioStreamMP3>(stream.ptr());
      mp3 != nullptr) {
    mp3->set_loop(loop);
    return;
  }
  if (godot::AudioStreamOggVorbis* const ogg =
          godot::Object::cast_to<godot::AudioStreamOggVorbis>(stream.ptr());
      ogg != nullptr) {
    ogg->set_loop(loop);
    return;
  }
  if (godot::AudioStreamWAV* const wav = godot::Object::cast_to<godot::AudioStreamWAV>(stream.ptr());
      wav != nullptr) {
    wav->set_loop_mode(loop ? godot::AudioStreamWAV::LOOP_FORWARD
                            : godot::AudioStreamWAV::LOOP_DISABLED);
  }
}

godot::Ref<godot::AudioStreamWAV> make_wav(const std::string& bytes) {
  const auto u16 = [&](std::size_t off) -> unsigned {
    return static_cast<unsigned>(static_cast<unsigned char>(bytes[off])) |
           (static_cast<unsigned>(static_cast<unsigned char>(bytes[off + 1])) << 8);
  };
  const auto u32 = [&](std::size_t off) -> unsigned {
    return u16(off) | (u16(off + 2) << 16);
  };
  if (bytes.size() < 44) return godot::Ref<godot::AudioStreamWAV>();
  std::size_t pos = 12;
  unsigned channels = 0, rate = 0, bits = 0, fmt_tag = 0;
  std::size_t data_off = 0, data_len = 0;
  while (pos + 8 <= bytes.size()) {
    const unsigned size = u32(pos + 4);
    const std::string id = bytes.substr(pos, 4);
    const std::size_t body = pos + 8;
    if (id == "fmt " && body + 16 <= bytes.size()) {
      fmt_tag = u16(body);
      channels = u16(body + 2);
      rate = u32(body + 4);
      bits = u16(body + 14);
    } else if (id == "data") {
      data_off = body;
      data_len = std::min<std::size_t>(size, bytes.size() - body);
    }
    pos = body + size + (size & 1u);
  }
  if (data_len == 0 || rate == 0 || channels == 0 || (bits != 8 && bits != 16)) {
    return godot::Ref<godot::AudioStreamWAV>();
  }
  godot::Ref<godot::AudioStreamWAV> wav;
  wav.instantiate();
  wav->set_data(to_packed(bytes.substr(data_off, data_len)));
  wav->set_format(fmt_tag == 3 ? godot::AudioStreamWAV::FORMAT_16_BITS
                               : (bits == 8 ? godot::AudioStreamWAV::FORMAT_8_BITS
                                            : godot::AudioStreamWAV::FORMAT_16_BITS));
  wav->set_mix_rate(static_cast<int32_t>(rate));
  wav->set_stereo(channels >= 2);
  return wav;
}

}  // namespace

void GodotAudio::install(godot::Node& owner, LfwGodotHost& host) {
  if (_owner != nullptr) return;
  _owner = &owner;
  _host = &host;

  godot::AudioServer* const server = godot::AudioServer::get_singleton();
  _slots.resize(kSlots);
  _pans.resize(kSlots);
  _ids.assign(kSlots, std::string());
  for (int i = 0; i < kSlots; ++i) {
    godot::AudioStreamPlayer* const player = memnew(godot::AudioStreamPlayer);
    player->set_name(godot::String("lfw_sfx_") + godot::String::num_int64(i));
    owner.add_child(player);
    godot::Ref<godot::AudioEffectPanner> panner;
    panner.instantiate();
    panner->set_pan(0.0f);
    if (server != nullptr) {
      const int bus = server->get_bus_count();
      server->add_bus();
      const godot::String bus_name = godot::String("lfw_sfx_") + godot::String::num_int64(i);
      server->set_bus_name(bus, bus_name);
      server->add_bus_effect(bus, panner);
      player->set_bus(bus_name);
    }
    _slots[static_cast<std::size_t>(i)] = player;
    _pans[static_cast<std::size_t>(i)] = panner;
  }
  _bgm = memnew(godot::AudioStreamPlayer);
  _bgm->set_name("lfw_bgm");
  owner.add_child(_bgm);

  LfwGodotHost::AudioSink sink;
  sink.preload = [this](const std::string& path) { preload(path); };
  sink.play = [this](const std::string& id, const std::string& path, double gain, double pan) {
    play(id, path, gain, pan);
  };
  sink.set_gain = [this](const std::string& id, double gain, double pan) {
    set_gain(id, gain, pan);
  };
  sink.stop_all = [this]() { stop_all(); };
  sink.play_bgm = [this](const std::string& path, bool loop, double gain) {
    play_bgm(path, loop, gain);
  };
  sink.set_bgm_gain = [this](double gain) { set_bgm_gain(gain); };
  sink.stop_bgm = [this]() { stop_bgm(); };
  host.set_audio_sink(std::move(sink));

  const std::string driver = server != nullptr
                                 ? std::string(server->get_driver_name().utf8().get_data())
                                 : std::string("<none>");
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "[godot] sounds: mixer %d sfx slots + 1 bgm (per-slot bus + AudioEffectPanner) "
                "driver=%s mix=%.0fHz",
                kSlots, driver.c_str(),
                server != nullptr ? static_cast<double>(server->get_mix_rate()) : 0.0);
  godot::UtilityFunctions::print(godot::String::utf8(buf));
}

std::string GodotAudio::stream_key(const std::string& path) {
  if (path.empty()) return std::string();
  const auto found = _cache.find(path);
  if (found != _cache.end()) {
    return found->second.is_valid() ? path : std::string();
  }
  _last_path = path;
  std::string bytes;
  GodotWorldRenderer* const renderer = _host != nullptr ? _host->world_renderer() : nullptr;
  const bool read = _read ? _read(path, bytes)
                          : (renderer != nullptr && renderer->read_resource(path, bytes));
  if (!read || bytes.empty()) {
    ++_failed;
    _cache[path] = godot::Ref<godot::AudioStream>();
    godot::UtilityFunctions::printerr(
        godot::String::utf8(("[godot] sounds: read failed: " + path).c_str()));
    return std::string();
  }
  godot::PackedByteArray data = to_packed(bytes);
  godot::Ref<godot::AudioStream> stream;
  if (looks_like_mp3(bytes)) {
    godot::Ref<godot::AudioStreamMP3> mp3;
    mp3.instantiate();
    mp3->set_data(data);
    stream = mp3;
  } else if (bytes_start_with(bytes, "OggS", 4)) {
    stream = godot::AudioStreamOggVorbis::load_from_buffer(data);
  } else if (bytes_start_with(bytes, "RIFF", 4)) {
    stream = make_wav(bytes);
  }
  if (stream.is_null()) {
    ++_failed;
    godot::UtilityFunctions::printerr(
        godot::String::utf8(("[godot] sounds: unsupported bytes: " + path).c_str()));
  } else {
    ++_decoded;
  }
  _cache[path] = stream;
  return stream.is_valid() ? path : std::string();
}

void GodotAudio::play(const std::string& id, const std::string& path, double gain, double pan) {
  if (_slots.empty()) return;
  const std::string key = stream_key(path);
  if (key.empty()) return;
  const godot::Ref<godot::AudioStream>& stream = _cache[key];
  int slot = -1;
  for (int i = 0; i < static_cast<int>(_slots.size()); ++i) {
    if (!_slots[static_cast<std::size_t>(i)]->is_playing()) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    slot = _next;
    _next = (_next + 1) % static_cast<int>(_slots.size());
    const std::string& stolen = _ids[static_cast<std::size_t>(slot)];
    if (!stolen.empty() && _host != nullptr) _host->on_audio_finished(stolen);
  }
  godot::AudioStreamPlayer* const player = _slots[static_cast<std::size_t>(slot)];
  if (player == nullptr) return;
  player->set_stream(stream);
  player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
  if (_pans[static_cast<std::size_t>(slot)].is_valid()) {
    _pans[static_cast<std::size_t>(slot)]->set_pan(
        static_cast<float>(std::max(-1.0, std::min(1.0, pan))));
  }
  player->play();
  _ids[static_cast<std::size_t>(slot)] = id;
}

void GodotAudio::set_gain(const std::string& id, double gain, double pan) {
  for (int i = 0; i < static_cast<int>(_ids.size()); ++i) {
    if (_ids[static_cast<std::size_t>(i)] != id) continue;
    godot::AudioStreamPlayer* const player = _slots[static_cast<std::size_t>(i)];
    if (player == nullptr) continue;
    player->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
    if (_pans[static_cast<std::size_t>(i)].is_valid()) {
      _pans[static_cast<std::size_t>(i)]->set_pan(
          static_cast<float>(std::max(-1.0, std::min(1.0, pan))));
    }
    return;
  }
}

void GodotAudio::preload(const std::string& path) { (void)stream_key(path); }

void GodotAudio::play_bgm(const std::string& path, bool loop, double gain) {
  if (_bgm == nullptr) return;
  const std::string key = stream_key(path);
  if (key.empty()) return;
  const godot::Ref<godot::AudioStream>& stream = _cache[key];
  set_stream_loop(stream, loop);
  _bgm->set_stream(stream);
  _bgm->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
  _bgm->play();
  _bgm_was_playing = true;
}

void GodotAudio::set_bgm_gain(double gain) {
  if (_bgm == nullptr) return;
  _bgm->set_volume_linear(static_cast<float>(std::max(0.0, std::min(1.0, gain))));
}

void GodotAudio::stop_bgm() {
  if (_bgm != nullptr) _bgm->stop();
  _bgm_was_playing = false;
}

void GodotAudio::stop_all() {
  for (std::size_t i = 0; i < _slots.size(); ++i) {
    if (_slots[i] != nullptr) _slots[i]->stop();
    if (i < _ids.size()) _ids[i].clear();
  }
  stop_bgm();
}

void GodotAudio::poll(bool log) {
  if (_host == nullptr) return;
  for (std::size_t i = 0; i < _slots.size() && i < _ids.size(); ++i) {
    if (_ids[i].empty() || _slots[i] == nullptr) continue;
    if (_slots[i]->is_playing()) continue;
    const std::string id = _ids[i];
    _ids[i].clear();
    _host->on_audio_finished(id);
  }
  if (_bgm != nullptr) {
    const bool playing = _bgm->is_playing();
    if (_bgm_was_playing && !playing && _host->audio_bgm_random() && _finished) {
      _bgm_was_playing = false;
      _finished();
    }
    _bgm_was_playing = playing || _bgm_was_playing;
  }
  const std::string text = report();
  if (text != _report) {
    _report = text;
    if (log) godot::UtilityFunctions::print(godot::String::utf8(("[godot] sounds: " + text).c_str()));
  }
}

std::string GodotAudio::report() const {
  if (_host == nullptr) return std::string();
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "preload=%lld decode=%lld fail=%lld play=%lld bgm=%lld active=%d bgm_playing=%d "
                "random=%d bgm_path=%s bgm_gain=%.3f last=%s",
                _host->audio_loaded(), _decoded, _failed, _host->audio_played(),
                _host->audio_bgm_played(), _host->audio_active(), bgm_playing() ? 1 : 0,
                _host->audio_bgm_random() ? 1 : 0,
                _host->audio_bgm_path().empty() ? "-" : _host->audio_bgm_path().c_str(),
                _host->audio_bgm_gain(),
                _host->audio_last().empty() ? "-" : _host->audio_last().c_str());
  return std::string(buf);
}

bool GodotAudio::bgm_playing() const { return _bgm != nullptr && _bgm->is_playing(); }

}
