#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "host/fs_zip.h"
#include "host/common/zip_file.h"
#include "host/godot/lfw_godot_audio.h"
#include "host/godot/lfw_godot_console.h"
#include "host/godot/lfw_godot_font.h"
#include "host/godot/lfw_godot_host.h"
#include "host/godot/lfw_godot_models.h"
#include "host/godot/lfw_godot_renderer.h"
#include "host/godot/lfw_godot_steam.h"
#include "host/godot/lfw_godot_textures.h"
#include "host/godot/lfw_godot_native_frame.h"
#include "host/godot/lfw_godot_titlebar.h"
#include "lfw/base/clock.h"
#include "lfw/lfw.h"
#include "lfw/ui/uinode.h"

namespace lfw::host {

using godot::Node2D;

class LfwGodotUiRun;

/** 控制台脚本条目：`--console-exec` / `--console-text` 的 `<文本>[@<帧>]` */
struct ConsoleSpec {
  std::string line;
  int frame = 0;
  bool done = false;
};

class LfwGodot : public Node2D {
  GDCLASS(LfwGodot, Node2D)
  /** 绘制段层借用宿主的绘制函数（同一份画法，只是画到各自的 canvas item 上） */
  friend class LfwGodotUiRun;

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
    /** `--console`：启动时就把控制台打开（诊断 / 截图用） */
    bool console = false;
    /** `--console-echo`：把控制台收到的日志行同时打到 stdout（headless 验收用） */
    bool console_echo = false;
    /** `--console-exec <行>[@<帧>]`：启动后（或指定帧）按控制台输入执行这些命令，可重复 */
    std::vector<ConsoleSpec> console_exec;
    /** `--console-text <文本>[@<帧>]`：只把文本填进控制台输入框（不执行），诊断建议列表用 */
    std::vector<ConsoleSpec> console_text;
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
    /** `--stats`：启动时就把左上角性能面板打开（诊断 / 截图用） */
    bool stats = false;
    /** `--steam=0|1|auto`：是否接 Steamworks（编进来了才能真开；缺 DLL / 没登录 = 只是关掉） */
    SteamMode steam = SteamMode::kAuto;
    /** `--steam-overlay`：初始化成功后立刻弹一次 Overlay（诊断 / 截图用） */
    bool steam_overlay = false;
    std::u16string ui_page;
  /** `--ui-scroll <node-id>:<px>`：把某个 ScrollView 滚到指定偏移（截图 / 验收用） */
  std::string ui_scroll;
    /** `--sync-render <0..4>`：强制渲染频率模式（0 Unlimited / 1 Half / 2 Sync / 3 FPS_60 / 4 FPS_120） */
    int sync_render = -1;
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
    /** `--drag-window x0,y0,x1,y1[@frame]`：窗口像素的左键拖拽（选字 / 拖动验收用） */
    struct DragAt {
      int frame = 0;
      double x0 = 0;
      double y0 = 0;
      double x1 = 0;
      double y1 = 0;
    };
    std::vector<DragAt> drags;
    std::vector<std::string> drag_specs;
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
  /** `--console-exec`：`frame < 0` = 跑启动时那批；否则只跑排在这一帧的 */
  void run_console_scripts(int frame);
  void render_world_frame(long long renders_before);
  /** 帧末兜底渲染：`FPS_60`/`FPS_120`/`Unlimited` 由帧回调按目标帧率补，`Sync`/`Half` 交给世界自己 */
  bool render_fallback_due(double real_ms);

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
  /** 窗口像素的左键拖拽：按下 → 6 段移动 → 松开（控制台选字 / 拖动验收用） */
  void push_window_drag(double x0, double y0, double x1, double y1);
  std::string ui_pages_text() const;
  void apply_ui_scroll();
  void draw_item(godot::CanvasItem& canvas, const GodotDrawItem& item);
  /** 当前帧画布变换（`GodotDrawItem` 的坐标都在这个空间里，绘制段层也要用它） */
  const godot::Transform2D& item_xform() const { return _item_xform; }
  /** 控制台 / 标题栏画到指定的 canvas item 上（绘制段层用） */
  void draw_console_into(godot::CanvasItem& canvas);
  void draw_titlebar_into(godot::CanvasItem& canvas);
  void draw_stats_into(godot::CanvasItem& canvas);
  std::string stats_text() const;
  bool stats_visible() const { return _stats_visible; }
  void set_stats_visible(bool visible);
  /** 按裁剪矩形把当帧命令切成若干段，分别塞进独立的 canvas item（见 `LfwGodotUiRun`） */
  void assign_ui_runs(const GodotFrame& frame);
  godot::Transform2D _item_xform;
  void draw_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item);
  void draw_text_run(godot::CanvasItem& canvas, const godot::Ref<godot::Font>& font,
                     const godot::Vector2& pos,
                     const std::string& line, int font_size, const godot::Color& color,
                     bool has_stroke, double stroke_w, const godot::Color& stroke_color);
  void draw_world_text_item(godot::CanvasItem& canvas, const GodotDrawItem& item,
                            const godot::Ref<godot::Font>& font,
                            int font_size);
  void draw_clip_probe(const godot::Transform2D& contain);
  void apply_window_setup();
  bool titlebar_visible() const;
  bool titlebar_drawn() const {
    return _args.titlebar == TitlebarMode::custom || _args.titlebar == TitlebarMode::frame;
  }
  bool native_frame() const { return _args.titlebar == TitlebarMode::frame; }
  bool titlebar_consume(const godot::Ref<godot::InputEvent>& event);
  void draw_titlebar(godot::CanvasItem& canvas);
  lfw::host::TitleBarConditions titlebar_conditions() const;
  bool titlebar_checked(lfw::host::TitleAction action) const;
  void titlebar_activate(int index);
  double titlebar_volume(int index) const;
  void titlebar_set_volume(int index, double v);
    void titlebar_update_hover(const lfw::host::TitleBarLayout& layout,
                               const lfw::host::TitleHit& hit, double bx, double by);
  void draw_rounded_rect(godot::CanvasItem& canvas, const lfw::host::TitleBarRect& r,
                         double radius, const godot::Color& color);
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

  Args _args;
  std::unique_ptr<LfwGodotHost> _host;
  GodotAudio _audio;
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

  /** 控制台（Ctrl+`）：输入框 / 日志 / 滚动条都在 `GodotConsole` 里，这里只做接线 */
  void _on_console_submitted(const godot::String& text);
  void _on_console_scroll(double value);
  void release_game_keys();
  lfw::host::GodotConsole _console;
  lfw::host::SteamRuntime _steam;
  bool _steam_overlay_pending = false;
  bool _steam_overlay_warned = false;
  /** 绘制段层（按裁剪矩形切出来的独立 canvas item），不够用时按需扩 */
  std::vector<LfwGodotUiRun*> _ui_runs;
  /** 当帧命令的绘制顺序（world + UI 按 pass 排好），前 `_parent_draw_count` 条由宿主自己画 */
  std::vector<const GodotDrawItem*> _draw_order;
  size_t _parent_draw_count = 0;
  bool _ui_scroll_applied = false;
  /** 距离下一次兜底渲染还有多少毫秒（按 `fps_value()` 的帧间隔倒计时） */
  double _render_wait_ms = 0.0;

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
  bool _stats_visible = false;
  double _step_cost_ms = 0.0;
  double _draw_cost_ms = 0.0;
  lfw::host::TitleBarState _bar;
  int _cursor_shape = -1;
  bool _maximized = false;
  godot::Vector2i _restore_size{0, 0};
  godot::Vector2i _restore_pos{0, 0};
};

}

