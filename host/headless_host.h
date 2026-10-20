#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "host/common/ui_input_handle.h"
#include "host/common/zip_file.h"
#include "lfw/base/clock.h"
#include "lfw/ditto/xml/tool_xml.h"
#include "lfw/lfw.h"
#include "lfw/world.h"

namespace lfw::host {

class ManualClock : public IClock {
 public:
  double now() const override { return _t; }
  int add(std::function<void()>) override { return 0; }
  void del(int) override {}
  bool hidden() const override { return false; }
  void advance(double dt) { _t += dt; }
  void set(double t) { _t = t; }

 private:
  double _t = 0;
};

class NoopTimeout : public ITimeout {
 public:
  int add(std::function<void()>, double) override { return 0; }
  void del(int) override {}
};

class NullWorldRenderer : public IWorldRenderer {
 public:
  void add_entity(Entity&) override { ++added; }
  void del_entity(Entity&) override { ++removed; }
  void render(double) override { ++renders; }
  void dispose() override {}

  long long added = 0;
  long long removed = 0;
  long long renders = 0;
};

class HeadlessHost : public ILfwHost {
 public:
  HeadlessHost(std::vector<std::filesystem::path> roots, bool dev);

  ManualClock& clock() { return _clock; }
  NoopTimeout& timeout() { return _timeout; }
  NullWorldRenderer& renderer() { return _renderer; }

  long long ignored_images() const { return _ignored_images; }
  int warn_count() const { return _warns; }
  int error_count() const { return _errors; }
  const std::vector<std::u16string>& last_errors() const { return _last_errors; }

  std::filesystem::path resolve(const std::u16string& path) const;

  void sounds_init(LFW&) override {}
  void images_init(LFW&) override {}
  void keyboard_init(LFW&) override {}
  void pointings_init(LFW&) override {}
  void keyboard_add_callback(LFW& lfw) override { keyboard_callback_add(lfw, &lfw); }
  void pointings_add_ui_input(LFW&) override;
  void keyboard_callback_add(LFW&, LfwKeyboardCallback* cb) override;
  void keyboard_callback_del(LFW&, LfwKeyboardCallback* cb) override;
  void pointings_callback_add(LFW&, LfwPointingsCallback* cb) override;
  void pointings_callback_del(LFW&, LfwPointingsCallback* cb) override;
  bool pointings_grab(LFW& lfw, const void* who) override;
  void pointings_ungrab(LFW& lfw, const void* who) override;
  const void* pointings_grabbing(LFW& lfw) override;
  void cache_forget(const std::u16string& type, double version) override;
  void zip_forget_stored(const std::u16string& type, double version) override;
  IWorldRenderer* create_world_renderer(LFW&) override { return &_renderer; }
  void load_img(const std::u16string& path) override;
  bool ui_image_load(const std::u16string& img_key, const Value& path, const Value& ops, Value& out,
                     std::u16string& error) override;
  void ui_image_pin(const std::u16string& img_key) override;
  std::u16string md5(const std::u16string& text) override;
  bool dev() const override { return _dev; }
  double now() override { return _clock.now(); }
  void warn(const std::vector<Value>& args) override;
  void error(const std::vector<Value>& args) override;
  void log(const std::vector<Value>& args) override;
  void debug_msg(const std::vector<Value>& args) override;
  std::function<void()> sounds_play_bgm(const Value& music) override;
  void sounds_stop_bgm() override {}
  void sounds_play(const Value& path, const Value& x, const Value& y, const Value& z) override;
  void sounds_play_preset(const Value& name, const Value& x, const Value& y,
                          const Value& z) override;
  void sounds_play_with_load(const Value& path) override;
  void sounds_load(const Value& name, const Value& src) override;
  std::u16string sounds_bgm() override { return _bgm; }
  bool sounds_is_random() override { return _bgm_random; }
  double sounds_volume() override { return _volume; }
  void sounds_set_volume(double v) override { _volume = v; }
  double sounds_bgm_volume() override { return _bgm_volume; }
  void sounds_set_bgm_volume(double v) override { _bgm_volume = v; }
  double sounds_sound_volume() override { return _sound_volume; }
  void sounds_set_sound_volume(double v) override { _sound_volume = v; }
  bool sounds_bgm_muted() override { return _bgm_muted; }
  void sounds_set_bgm_muted(bool v) override { _bgm_muted = v; }
  bool sounds_sound_muted() override { return _sound_muted; }
  void sounds_set_sound_muted(bool v) override { _sound_muted = v; }
  void sounds_callback_add(LFW&, LfwSoundsCallback* cb) override;
  void sounds_callback_del(LFW&, LfwSoundsCallback* cb) override;
  void sounds_dispose() override {}
  void keyboard_dispose() override {}
  void pointings_dispose() override;
  Value measure_text(const Value& text, const Value& style) override;
  void player_cache_get(const std::u16string& name, PlayerInfoCacheEntry& out) override;
  bool player_cache_del(const std::u16string& name, std::u16string& error) override;
  void player_cache_put(const PlayerInfoCachePut& data) override;
  bool import_as_json(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                      std::u16string& error) override;
  bool import_as_blob_url(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                          std::u16string& error) override;
  bool import_as_array_buffer(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                              std::u16string& error) override;
  bool import_as_image_bitmap(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                              std::u16string& error) override;
  bool import_as_text(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                      std::u16string& error) override;
  bool xml_parse(const Value& text, Value& marker, std::shared_ptr<IXMLElement>& root,
                 std::u16string& error) override;
  void lang_apply(LFW& lfw, const std::u16string& lang, const std::u16string& prev) override;
  Value zip_get_stored(const std::u16string& zip_url, const std::u16string& md5) override;
  bool zip_read_blob(const std::u16string& name, const Value& blob, const Value& md5, IZip*& out,
                     std::u16string& error) override;
  bool zip_read_buf(const std::u16string& name, const Value& data, IZip*& out,
                    std::u16string& error) override;
  bool zip_download(const std::u16string& zip_url, const Value& md5, LFW& lfw, DownloadedZip& out,
                    std::u16string& error) override;
  Value zip_cache_get(const std::u16string& name) override;
  void zip_cache_del(const std::u16string& name, const std::u16string& version) override;
  void zip_cache_put(const Value& entry) override;

  void emit_key_down(LfwKeyEvent& e);
  void emit_key_up(LfwKeyEvent& e);

  void key_down(const std::u16string& key, const std::u16string& device_type = u"keyboard");
  void key_up(const std::u16string& key, const std::u16string& device_type = u"keyboard");
  const std::map<std::u16string, int>& pressed_keys() const { return _key_times; }

  void emit_pointer_down(LfwPointingEvent& e);
  void emit_pointer_move(LfwPointingEvent& e);
  void emit_pointer_up(LfwPointingEvent& e);
  void emit_pointer_cancel(LfwPointingEvent& e);
  void emit_click(LfwPointingEvent& e);
  void emit_wheel(LfwPointingEvent& e);
  void emit_pointer_enter(LfwPointingEvent& e);
  void emit_pointer_leave(LfwPointingEvent& e);

  UiInputHandle* ui_input() const { return _ui_input.get(); }

  void click_ui(LFW& lfw, double x, double y, double button = 0.0);

 private:
  bool read_file(const std::filesystem::path& path, std::string& out, std::u16string& error) const;

  struct Blob {
    std::u16string token;
    std::u16string md5;
    std::vector<uint8_t> bytes;
  };
  std::string blob_token(const Blob& blob);
  const Blob* blob_of(const Value& blob) const;

  std::vector<std::filesystem::path> _roots;
  bool _dev = false;
  ManualClock _clock;
  NoopTimeout _timeout;
  NullWorldRenderer _renderer;
  ToolXML _xml;
  std::vector<LfwKeyboardCallback*> _kb_callbacks;
  std::vector<LfwPointingsCallback*> _pt_callbacks;
  std::unique_ptr<UiInputHandle> _ui_input;
  std::map<std::u16string, int> _key_times;
  std::map<std::string, Blob> _blobs;
  std::map<std::string, std::u16string> _stored;
  long long _blob_seq = 0;
  std::vector<std::unique_ptr<ZipFile>> _zip_objs;
  long long _grab_prev_epoch = -1;
  long long _grab_epoch = 0;
  const void* _grabbing = nullptr;
 protected:
  std::vector<LfwSoundsCallback*> _sd_callbacks;
  std::u16string _bgm;
  bool _bgm_random = false;
  bool _bgm_muted = false;
  bool _sound_muted = false;
  double _volume = 1.0;
  double _bgm_volume = 1.0;
  double _sound_volume = 1.0;

 private:
  std::map<std::u16string, PlayerInfoCachePut> _player_cache;
  std::map<std::u16string, Value> _zip_cache;
  long long _ignored_images = 0;
  int _warns = 0;
  int _errors = 0;
  int _logs = 0;
  std::vector<std::u16string> _last_errors;
};

}
