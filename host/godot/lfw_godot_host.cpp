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
#include "host/godot/lfw_godot_ws.h"
#include "host/net/net_client.h"
#include "host/net/net_frame_driver.h"
#include "host/net/net_sync.h"
#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/value.h"
#include "lfw/defines/defines_data.h"
#include "lfw/defines/facing_flag.h"
#include "lfw/defines/game_mode.h"
#include "lfw/entity/entity.h"
#include "lfw/ground.h"
#include "lfw/loader/dat_mgr.h"
#include "lfw/player_info.h"
#include "lfw/ui/uilayer.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"
#include "lfw/world.h"

namespace lfw::host {

namespace {

std::u16string u16_or(const Value& v, const std::u16string& fallback) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : fallback;
}

/** 联机座位：`<client_id>#<1..4>`（对齐 Web 端 `player_id`）。 */
std::u16string net_player_id(const lfw::net::NetRoomClient& member, int slot) {
  return member.info.id + u"#" + std::u16string(1, static_cast<char16_t>(u'0' + slot));
}

/**
 * 这个座位是否参战：房间还没选人时每个客户端只上 1P，
 * 选过人就按 `players` 里填了名字的座位来（对齐 Web 端的数据形状）。
 */
bool net_slot_used(const lfw::net::NetRoomClient& member, int slot) {
  if (member.info.players.empty()) return slot == 1;
  const size_t index = static_cast<size_t>(slot - 1);
  return index < member.info.players.size() && !member.info.players[index].empty();
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

std::string to_utf8(const std::u16string& s) {
  const std::vector<uint8_t> buf = lfw::encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

}

LfwGodotHost::LfwGodotHost(std::vector<std::filesystem::path> roots, bool dev)
    : HeadlessHost(std::move(roots), dev) {
  _volume = 0.3;
  _bgm_volume = 0.5;
  _sound_volume = 1.0;
}

lfw::net::INetService* LfwGodotHost::net_service() {
  if (_net_client == nullptr) {
    _net_client = std::make_unique<net::NetClient>(std::make_unique<GodotWsTransport>());
    _net_client->set_room_start_sink([this](const Value& resp) { net_on_room_start(resp); });
    _net_client->set_frame_sink([this](const Value& resp) { net_on_tick(resp); });
    _net_client->set_logger([this](const std::string& text) {
      log({Value(from_utf8("[net] " + text))});
    });
  }
  return _net_client.get();
}

void LfwGodotHost::net_poll() {
  if (_net_client != nullptr) _net_client->poll();
  // 对齐 Web 端 `NetworkLogic::release_world()`：连接真的没了（不是重连中）就把帧循环
  // 交还本地，否则 `can_step()` 永远等不到 Tick，整个界面会冻住。
  if (_net_driver != nullptr && _net_client != nullptr && !_net_client->opened() &&
      !_net_client->connecting() && !_net_client->reconnecting()) {
    net_release_world();
  }
}

/** 对齐 Web 端 `NetworkLogic::release_world()`：摘掉网络帧驱动，让世界恢复本地推进。 */
void LfwGodotHost::net_release_world() {
  _net_driver.reset();
  _net_lfw = nullptr;
  _net_started = false;
  _net_last_seq = -1;
}

bool LfwGodotHost::can_step() const {
  if (_net_driver != nullptr) return _net_driver->can_step();
  return _accumulator_ms >= _step_ms;
}

void LfwGodotHost::net_on_room_start(const Value& resp) {
  // 注意：`_net_lfw` 就是在这里赋值的，别用它做前置判断（第一次开局时它还是空）
  if (_net_client == nullptr || _lfw == nullptr) return;
  const double seed = to_number(field_or(resp, u"seed"));
  const std::u16string mode = u16_or(field_or(resp, u"sync_mode"), u"auto");
  net::SyncChoice choice = net::SyncChoice::kAuto;
  net::parse_sync_choice(mode, choice);
  const net::SyncPlan plan = net::resolve_sync(choice, _net_client->rtt(), net::kNetUps);

  if (plan.mode == net::SyncMode::kLockstep) {
    _net_driver = std::make_unique<net::LockstepDriver>();
  } else {
    _net_driver = std::make_unique<net::DelayDriver>(plan.input_delay);
  }
  _net_driver->start(0);
  _net_last_seq = -1;
  _net_started = false;
  _net_lfw = _lfw;

  World& world = _lfw->world();
  world.dataset.set(u"UPS", Value(60.0));
  world.dataset.set(u"atom_time", Value(1.0));
  world.dataset.set(u"wait_offset", Value(0.0));
  world.dataset.set(u"fvy_f", Value(-0.5));
  world.dataset.set(u"double_click_interval", Value(30.0));
  world.dataset.set(u"key_hit_duration", Value(10.0));
  _lfw->mt_ref().reset(std::isnan(seed) ? 0.0 : seed);
  // id 计数器要重置到 100，让两端从同一段 id 起算；上一局残留的实体必须先回收，
  // 否则新角色拿到的 id 会和残留实体撞号，`World::add_entities` 会静默丢掉它。
  world.purge_entities();
  _lfw->reset_new_id();
  _lfw->reset_new_team();
  net_prepare_players();

  if (ui::UILayers* const layers = _lfw->ui_layers()) {
    lfw::Object opts;
    opts.set(u"id", lfw::Value(std::u16string(u"network_loading")));
    layers->set_page(lfw::Value(std::make_shared<lfw::Object>(opts)), 0.0);
  }

  // Web 端在 `LFW.on_loading_end` 里发第 0 帧（`NetworkLogic.lfw_cb`）：
  // 服务端收齐所有客户端同一 seq 才广播，谁都不发就永远没有帧 ⇒ 一直卡在加载页。
  net_prime_ticks();
}

/** 建 `client#座位` 的玩家槽位（只有本机的 4 个座位是 `local`）。 */
void LfwGodotHost::net_prepare_players() {
  if (_net_client == nullptr || _net_lfw == nullptr) return;
  const std::optional<lfw::net::NetRoomInfo>& room = _net_client->room();
  if (!room.has_value()) return;
  const std::u16string my_id =
      _net_client->client().has_value() ? _net_client->client()->id : std::u16string();
  for (const lfw::net::NetRoomClient& member : room->clients) {
    const bool local = !my_id.empty() && member.info.id == my_id;
    for (int slot = 1; slot <= 4; ++slot) {
      if (!net_slot_used(member, slot)) continue;
      const size_t index = static_cast<size_t>(slot - 1);
      const std::u16string name = index < member.info.players.size() &&
                                          !member.info.players[index].empty()
                                      ? member.info.players[index]
                                      : std::u16string(1, static_cast<char16_t>(u'0' + slot));
      _net_lfw->player(net_player_id(member, slot), lfw::Value(name), lfw::Value(local),
                       lfw::Value(local));
      if (local) {
        // 默认键位表是按座位号 `1..4` 建的（`Defines.default_keys_map`）。联机玩家 id 是
        // `client_xxx#1`，查不到会退到占位键位 `_`（`_L`/`_a`…），于是本地按键永远
        // 匹配不上玩家 —— 「按攻击加入」按了没反应。这里按座位号覆盖回来。
        _net_lfw->player(net_player_id(member, slot))
            ->set_keys(defines::get_default_keys_value(
                std::u16string(1, static_cast<char16_t>(u'0' + slot))));
      }
    }
  }
}

/** 补发 `[0, lead)` 的 Tick：lockstep 只有第 0 帧，delay 要先垫 `lead-1` 帧空输入。 */
void LfwGodotHost::net_prime_ticks() {
  if (_net_client == nullptr) return;
  if (!_net_client->opened()) return;
  const int lead = _net_driver != nullptr ? _net_driver->lead() : 1;
  for (int seq = 0; seq < std::max(1, lead); ++seq) {
    lfw::Object payload;
    payload.set(u"seq", lfw::Value(static_cast<double>(seq)));
    _net_client->send_nowait(u"Tick", lfw::Value(std::make_shared<lfw::Object>(payload)));
  }
}

/** 第一帧到了：离开加载页、生成各座位角色、进入对局（对齐 Web 端 `LFWNetworkDriver::start`）。 */
void LfwGodotHost::net_start_match() {
  if (_net_started || _net_lfw == nullptr) return;
  _net_started = true;
  lfw::LFW& lfw = *_net_lfw;
  lfw.world().reset_game_time();

  // 对齐 Web 端 `LFWNetworkDriver::start()`：加载结束只回主页，**不**在这里生成角色、
  // 也不自动进对局 —— 模式和角色由玩家自己选（Web 端联机对局本体尚未实现）。
  if (ui::UILayers* const layers = lfw.ui_layers()) {
    ui::UILayer* const layer = layers->at(0.0);
    const lfw::Value cur =
        layer != nullptr && layer->page() != nullptr ? layer->page()->id() : lfw::Value();
    const std::u16string* const cur_id = std::get_if<std::u16string>(&cur);
    if (cur_id == nullptr || *cur_id == u"network_loading") {
      lfw::Object opts;
      opts.set(u"id", lfw::Value(std::u16string(u"main_page")));
      layers->set_page(lfw::Value(std::make_shared<lfw::Object>(opts)), 0.0);
    }
  }
}

std::vector<lfw::net::NetRoomClient> LfwGodotHost::net_clients() const {
  if (_net_client == nullptr || !_net_client->room().has_value()) return {};
  std::vector<lfw::net::NetRoomClient> out;
  // 同一个 client 在房间列表里可能出现重复（重连残留 / 服务端回声），
  // 直接按 id 去重，否则会建出两个同名同座位的玩家。
  for (const lfw::net::NetRoomClient& c : _net_client->room()->clients) {
    bool seen = false;
    for (const lfw::net::NetRoomClient& kept : out) {
      if (kept.info.id == c.info.id) {
        seen = true;
        break;
      }
    }
    if (!seen) out.push_back(c);
  }
  return out;
}

void LfwGodotHost::net_on_tick(const Value& resp) {
  if (_net_driver == nullptr) return;
  const double seq = to_number(field_or(resp, u"seq"));
  if (std::isnan(seq)) return;
  if (seq == 0.0) net_start_match();
  net::FrameInputs inputs;
  inputs.seq = static_cast<int>(seq);
  if (const Array* const reqs = as_array(field_or(resp, u"reqs"))) {
    for (const Value& req : reqs->items()) {
      net::ClientTick tick;
      tick.client_id = to_utf8(u16_or(field_or(req, u"client_id"), std::u16string()));
      tick.seq = static_cast<int>(to_number(field_or(req, u"seq")));
      if (const Array* const cmds = as_array(field_or(req, u"cmds"))) {
        for (const Value& cmd : cmds->items()) {
          const std::u16string* const s = std::get_if<std::u16string>(&cmd);
          if (s != nullptr) tick.cmds.push_back(to_utf8(*s));
        }
      }
      inputs.reqs.push_back(std::move(tick));
    }
  }
  _net_driver->push(std::move(inputs));
}

void LfwGodotHost::net_submit_local_cmds() {
  if (_net_client == nullptr || _net_driver == nullptr || _net_lfw == nullptr) return;
  if (!_net_client->opened()) return;
  const std::u16string client_id =
      _net_client->client().has_value() ? _net_client->client()->id : std::u16string();
  std::vector<Value> cmds;
  cmds.reserve(_net_lfw->cmds.size());
  for (const std::u16string& cmd : _net_lfw->cmds) {
    cmds.push_back(Value(client_id.empty() ? cmd : cmd + u" --from=" + client_id));
  }
  _net_lfw->cmds.clear();
  lfw::Object payload;
  payload.set(u"seq", lfw::Value(static_cast<double>(_net_driver->submit_seq())));
  payload.set(u"cmds", lfw::Value(std::make_shared<lfw::Array>(std::move(cmds))));
  _net_client->send_nowait(u"Tick", lfw::Value(std::make_shared<lfw::Object>(payload)));
}

LfwGodotHost::~LfwGodotHost() {
  if (_remote == nullptr || _remote->persist()) return;
  // Windows 上文件还开着就删不掉：先把懒读的包关掉再删临时目录
  _remote_zips.clear();
  std::error_code ec;
  std::filesystem::remove_all(_remote->dir(), ec);
}

void LfwGodotHost::push_frame(World& world) {
  if (_net_driver != nullptr) {
    std::optional<net::FrameInputs> frame = _net_driver->take();
    if (!frame.has_value()) return;
    // 对齐 Web 端 `run_tick` 的顺序：先把自己这一帧要发的 Tick 发出去（`seq + lead`），
    // 再把别的端的命令压回 `lfw.cmds`，最后才推进一帧。
    // 注意 `take()` 已经把 `_seq` 推到下一帧，所以 `submit_seq()` 正是 `seq + lead`。
    net_submit_local_cmds();
    for (const net::ClientTick& req : frame->reqs) {
      for (const std::string& cmd : req.cmds) _lfw->push_cmd_line(from_utf8(cmd));
    }
    _net_last_seq = frame->seq;
    if (_net_client != nullptr) _net_client->set_rejoin_seq(_net_last_seq + 1);
    clock().advance(_step_ms);
    world.update_once(_step_ms);
    ++_steps;
    return;
  }
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
  const std::size_t voices = _audio.max_voices > 0 && _playings.size() > static_cast<std::size_t>(_audio.max_voices)
                                 ? static_cast<std::size_t>(_audio.max_voices)
                                 : _playings.size();
  const double playings = static_cast<double>(voices) + 1.0;
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
