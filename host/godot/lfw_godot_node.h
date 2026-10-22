#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <godot_cpp/classes/audio_effect_panner.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "host/fs_zip.h"
#include "host/common/zip_file.h"
#include "host/godot/lfw_godot_font.h"
#include "host/godot/lfw_godot_host.h"
#include "host/godot/lfw_godot_models.h"
#include "host/godot/lfw_godot_renderer.h"
#include "host/godot/lfw_godot_textures.h"
#include "host/godot/lfw_godot_native_frame.h"
#include "host/godot/lfw_godot_titlebar.h"
#include "lfw/base/clock.h"
#include "lfw/lfw.h"
#include "lfw/ui/uinode.h"

namespace lfw::host {

using godot::Node2D;

class LfwGodot : public Node2D {
  GDCLASS(LfwGodot, Node2D)

 public:
  LfwGodot() = default;
  ~LfwGodot() override {
    lfw::set_clock(nullptr);
    lfw::set_timeout(nullptr);
  }

  void _ready() override;
  void _process(double p_delta) override;
  void _input(const godot::Ref<godot::InputEvent>& p_event) override;
  void _notification(int p_what);
  void _draw() override;

  /** 系统拖进来的文件（Godot 的 `Window.files_dropped` 信号） */
  void _on_files_dropped(const godot::PackedStringArray& files);
  /** 「附加模组」按钮 / 拖拽 都会走这里：把一批 zip 当模组热加载 */
  void load_dropped(const std::vector<std::filesystem::path>& files);
  /** 弹系统文件对话框选 zip（入口页「附加模组」/「添加模组」） */
  void open_mod_dialog();
  void _on_mod_files_selected(bool status, const godot::PackedStringArray& paths);
  /** 入口页「自定义游戏」：选一个 full.zip（内含 index.json → 包内 prel.zip/data.zip） */
  void open_custom_game_dialog();
  void _on_custom_game_selected(bool status, const godot::PackedStringArray& paths);
  /** 把选中的自定义游戏包暂存并重启（整包替换本体数据，运行中换不了） */
  bool switch_to_full_game(const std::filesystem::path& path);
  /** 打开自定义游戏包里的内嵌 zip（`prel.zip` / `data.zip` ...） */
  bool open_full_pack(ZipFile& zip, const std::string& label,
                      const std::vector<std::u16string>& paths, std::vector<IZip*>& out);

  enum class TitlebarMode { native, none, custom, frame };

 protected:
  static void _bind_methods();

 private:
  struct Args {
    std::vector<std::filesystem::path> roots;
    std::vector<std::filesystem::path> zips;
    /** `--mods` 额外模组目录（越后面优先），默认还会扫 `<exe>/mods`、`<cwd>/mods`、用户数据目录 `mods/` */
    std::vector<std::filesystem::path> mod_dirs;
    /** 扫出来的模组 zip（其中一个可能是「自定义游戏包」） */
    std::vector<std::filesystem::path> mod_zips;
    /** `--drop` 诊断：启动后按「拖拽」路径热加载这些 zip */
    std::vector<std::filesystem::path> drop_zips;
    /** `--custom-game` 诊断：启动后走「自定义游戏」路径（暂存 + 重启） */
    std::vector<std::filesystem::path> custom_games;
    bool no_mods = false;
    std::vector<std::u16string> fighters;
    std::u16string stage;
    int frames = 600;
    int players = 2;
    int every = 0;
    double time = 0.0;
    double delta = 0.0;
    bool vs = false;
    bool play = false;
    TitlebarMode titlebar = TitlebarMode::native;
    bool titlebar_given = false;
    double window_scale = 1.0;
    bool window_scale_given = false;
    int window_w = 0;
    int window_h = 0;
    double display_scale = 0.0;
    bool integer_scale = false;
    std::filesystem::path icons;
    bool icons_given = false;
    std::filesystem::path screenshot;
    int shot_frame = -1;
    bool ui_dump = false;
    bool clip_probe = false;
    std::u16string ui_page;
    std::u16string text_input;
    int text_input_frame = 60;
    bool text_input_given = false;
    std::u16string lang;
    std::string version_name;
    struct ClickAt {
      int frame = 0;
      double x = 0;
      double y = 0;
      bool window_space = false;
      bool hover = false;
    };
    std::vector<ClickAt> clicks;
    std::vector<std::string> click_specs;
    std::vector<std::string> window_click_specs;
    std::vector<std::string> hover_specs;
    int click_frame = 60;
    struct KeyAt {
      int frame = 0;
      bool down = true;
      std::u16string key;
    };
    struct KeySpec {
      std::string kind;
      std::string text;
    };
    std::vector<KeyAt> keys;
    std::vector<KeySpec> key_specs;
    int key_frame = 60;
    int key_tap = 2;
  };

  static const char* usage();
  static std::vector<std::string> user_args();
  bool parse_args(const std::vector<std::string>& argv, Args& out) const;
  bool setup();
  bool unlimited() const { return _args.frames <= 0; }
  void install_text_metrics();
  void report_frame(int frame) const;
  void render_world_frame(long long renders_before);

  void sync_text_input();
  void destroy_text_input();
  void probe_text_input(int frame);
  void ui_screen_size(double& w, double& h) const;
  void logical_size(double& w, double& h) const;
  godot::Transform2D contain_transform(const godot::Vector2i& window_size) const;
  /** 全屏 / 最大化时内容是整数倍缩放（像素画不糊）。 */
  bool pixel_integer_scaling() const;
  godot::Rect2 contain_rect(const godot::Vector2i& window_size) const;
  godot::Vector2i window_size_px() const;
  godot::Transform2D final_transform() const;
  void apply_game_clip(const godot::Vector2i& window_size);
  void apply_font_oversampling(const godot::Transform2D& contain);
  void report_stretch(const godot::Transform2D& contain, const godot::Rect2& content);
  enum class PointerKind { none, down, up, move, wheel };
  PointerKind to_pointing_event(const godot::Ref<godot::InputEvent>& event,
                                LfwPointingEvent& out) const;
  Args::ClickAt* inject_clicks(int frame);
  const Args::KeyAt* inject_keys(int frame);
  void push_window_click(double x, double y);
  void push_window_hover(double x, double y);
  std::string ui_pages_text() const;
  void draw_item(const GodotDrawItem& item);
  godot::Transform2D _item_xform;
  void draw_text_item(const GodotDrawItem& item);
  void draw_text_run(const godot::Ref<godot::Font>& font, const godot::Vector2& pos,
                     const std::string& line, int font_size, const godot::Color& color,
                     bool has_stroke, double stroke_w, const godot::Color& stroke_color);
  void draw_world_text_item(const GodotDrawItem& item, const godot::Ref<godot::Font>& font,
                            int font_size);
  void draw_clip_probe(const godot::Transform2D& contain);
  void apply_window_setup();
  bool titlebar_visible() const;
  bool titlebar_drawn() const {
    return _args.titlebar == TitlebarMode::custom || _args.titlebar == TitlebarMode::frame;
  }
  bool native_frame() const { return _args.titlebar == TitlebarMode::frame; }
  bool titlebar_consume(const godot::Ref<godot::InputEvent>& event);
  void draw_titlebar();
  lfw::host::TitleBarConditions titlebar_conditions() const;
  bool titlebar_checked(lfw::host::TitleAction action) const;
  void titlebar_activate(int index);
  double titlebar_volume(int index) const;
  void titlebar_set_volume(int index, double v);
    void titlebar_update_hover(const lfw::host::TitleBarLayout& layout,
                               const lfw::host::TitleHit& hit, double bx, double by);
  void draw_rounded_rect(const lfw::host::TitleBarRect& r, double radius, const godot::Color& color);
  double titlebar_window_width(const godot::Vector2i& window_size) const;
  double titlebar_display_scale() const;
  void setup_icons();
  void apply_window_icon();
  void report_titlebar_controls();
  void print_exit_summary() const;
  bool capture_screenshot();
  void finish();
  void quit_with(int code) const;

  void install_audio();
  std::string audio_stream_key(const std::string& path);
  void audio_play(const std::string& id, const std::string& path, double gain, double pan);
  void audio_set_gain(const std::string& id, double gain, double pan);
  void audio_preload(const std::string& path);
  void audio_play_bgm(const std::string& path, bool loop, double gain);
  void audio_set_bgm_gain(double gain);
  void audio_stop_bgm();
  void audio_stop_all();
  void audio_poll();
  std::string audio_report() const;

  static constexpr int kSfxSlots = 24;
  std::vector<godot::AudioStreamPlayer*> _sfx_slots;
  std::vector<godot::Ref<godot::AudioEffectPanner>> _sfx_pans;
  std::vector<std::string> _sfx_ids;
  int _sfx_next = 0;
  godot::AudioStreamPlayer* _bgm_player = nullptr;
  std::map<std::string, godot::Ref<godot::AudioStream>> _audio_cache;
  long long _audio_decoded = 0;
  long long _audio_failed = 0;
  std::string _audio_last_path;
  bool _bgm_was_playing = false;
  std::string _audio_report;

  Args _args;
  std::unique_ptr<LfwGodotHost> _host;
  std::vector<std::unique_ptr<FsZip>> _zips;
  /** 真 `.zip` 数据包（`--zip` 指到文件时走这条） */
  std::vector<std::unique_ptr<ZipFile>> _archives;
  std::unique_ptr<lfw::LFW> _lfw;
  std::unique_ptr<GodotTextureCache> _textures;
  std::unique_ptr<GodotTextureCache> _icons;
  std::unique_ptr<GodotModelBank> _models;
  std::map<std::string, godot::Ref<godot::ViewportTexture>> _model_textures;
  std::string _icons_dir;
  std::string _icon_report;
  lfw::host::FontCache _fonts;

  struct TextInputOverlay {
    godot::LineEdit* edit = nullptr;
    lfw::ui::UINode* node = nullptr;
    std::u16string shown;
    std::u16string synced;
    std::string style_sig;
    bool had_focus = false;
  };
  std::vector<TextInputOverlay> _text_inputs;
  TextInputOverlay* last_text_input();

  godot::Ref<godot::Font> font_for_css(const std::string& css);

  std::string resolve_version_name() const;

  void report_key_focus(const std::u16string& key);
  bool _running = false;
  bool _title_fixed = false;
  int _frames = 0;
  int _key_seq = 0;
  bool _capture_pending = false;
  bool _captured = false;
  bool _windowed = false;
  bool _native_frame = false;
  int _logical_w = 794;
  int _logical_h = 450;
  std::string _stretch_report;
  bool _bar_capture = false;
  bool _bar_dragging = false;
  lfw::host::TitleBarState _bar;
  int _cursor_shape = -1;
  bool _maximized = false;
  godot::Vector2i _restore_size{0, 0};
  godot::Vector2i _restore_pos{0, 0};
};

}

