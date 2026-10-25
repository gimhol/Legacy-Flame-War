#pragma once

#include <functional>
#include <string>
#include <vector>

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace lfw::host {

/**
 * 游戏内控制台（Ctrl+`）：一个真 `LineEdit` 负责输入，日志自己画在窗口上，右侧带滚动条。
 * 与宿主解耦：执行命令、取字体、开/关时的副作用都通过回调注入。
 */
class GodotConsole {
 public:
  using ExecFn = std::function<void(const std::string& line)>;
  using FontFn = std::function<godot::Ref<godot::Font>(const std::string& css)>;
  using HookFn = std::function<void()>;

  struct Line {
    std::string text;
    int kind = 0;  // 0 = 输入回显，1 = 普通输出，2 = 警告，3 = 错误
  };

  /** 建 CanvasLayer / LineEdit / ScrollBar（隐藏状态，可重复调用） */
  void create(godot::Node2D* owner);
  bool created() const { return _owner != nullptr; }

  void set_exec(ExecFn fn) { _exec = std::move(fn); }
  void set_font_provider(FontFn fn) { _font = std::move(fn); }
  /** 打开 / 关闭时的副作用（释放游戏按键、让出 UI 输入焦点…） */
  void set_open_hook(HookFn fn) { _on_open = std::move(fn); }
  void set_close_hook(HookFn fn) { _on_close = std::move(fn); }

  godot::LineEdit* edit() const { return _edit; }
  godot::VScrollBar* scroll_bar() const { return _bar; }

  bool is_open() const { return _open; }
  void open();
  void close();
  void toggle();

  /** 每帧：贴底摆放输入框、同步滚动条（窗口缩放后也跟着走） */
  void sync(const godot::Vector2i& window, const godot::Transform2D& final_transform);
  /** 在宿主的 `_draw()` 里画面板与日志 */
  void draw(godot::CanvasItem& canvas, const godot::Vector2i& window);

  /** 键盘：返回 true = 已消费（含关闭状态下的 Ctrl+` 开关） */
  bool handle_key(const godot::Ref<godot::InputEventKey>& key);
  /** 鼠标：返回 true = 已消费（滚轮翻页等） */
  bool handle_mouse(const godot::Ref<godot::InputEventMouseButton>& mouse);
  /** 鼠标移动：日志拖选（返回 true = 已消费，目前不消费） */
  bool handle_motion(const godot::Ref<godot::InputEventMouseMotion>& motion);

  /** 追加日志（`kind` 见 `Line`）；`\n` 会拆成多行 */
  void append(int kind, const std::string& text);
  /** 执行一行命令（回显 + 记历史 + 交给 `ExecFn`）；空行忽略，`clear`/`cls` 清屏 */
  void execute(const std::string& line);
  /** LineEdit 回车回调：执行并清空输入 */
  void submit(const std::string& line);
  /** 滚动条拖动 / 点击的回调 */
  void on_scrollbar_value(double value);

  /** 全选日志行（`Ctrl+A`）；`copy_selection()` 把选中的文本写进剪贴板，返回行数 */
  void select_all();
  int copy_selection();
  /** 选中了非空的一段（点一下不算） */
  bool has_selection() const {
    if (_sel_a.line < 0 || _sel_b.line < 0) return false;
    return _sel_a.line != _sel_b.line || _sel_a.col != _sel_b.col;
  }

 private:
  struct Metrics {
    float panel_h = 0.0f;
    int rows = 1;
    float input_top = 0.0f;
  };

  Metrics metrics(const godot::Vector2i& window) const;
  void scroll_by(int delta);
  void sync_scrollbar(const godot::Vector2i& window, const godot::Transform2D& final_transform);
  void history_step(int delta);
  int max_scroll() const;
  void request_redraw();
  /** 窗口像素 → 日志区的行号（`_vis_*` 是上一次绘制记下的可视区）；不在日志区返回 -1 */
  int line_at_window_pos(const godot::Vector2& pos) const;
  /** 窗口像素 → (行, 字符列)：列按 UTF-8 码点边界对齐，CJK 也是双宽按实测宽度算 */
  bool pos_at_window_pos(const godot::Vector2& pos, int& line, int& col) const;
  /** 一段文本前 `col` 个字节的显示宽度（`col` 已对齐到码点边界） */
  float text_width(const std::string& text, int col) const;
  /** x（相对文本起点）落在第几个码点边界上 */
  int col_at_x(const std::string& text, float x) const;
  void norm_selection(int& a_line, int& a_col, int& b_line, int& b_col) const;
  bool line_selected(int index) const;

  godot::Node2D* _owner = nullptr;
  godot::CanvasLayer* _layer = nullptr;
  godot::LineEdit* _edit = nullptr;
  godot::VScrollBar* _bar = nullptr;

  ExecFn _exec;
  FontFn _font;
  HookFn _on_open;
  HookFn _on_close;

  bool _open = false;
  godot::Vector2i _window{0, 0};
  godot::Transform2D _final;
  std::vector<Line> _lines;
  std::vector<std::string> _history;
  int _history_pos = -1;
  int _scroll = 0;  // 从底部往上滚了多少行（0 = 贴着最新一行）
  std::string _draft;

  /** 拖选：字符级（列 = UTF-8 码点序号），`a` 是按下处、`b` 是当前拖到的地方 */
  struct SelPos {
    int line = -1;
    int col = 0;
  };
  bool _dragging = false;
  SelPos _sel_a;
  SelPos _sel_b;
  int _copied_hint = 0;
  /** 上一次绘制记下的可视区，鼠标换行号要用 */
  int _vis_top = -1;
  int _vis_rows = 0;
  float _vis_text_top = 0.0f;
  float _vis_text_w = 0.0f;
  godot::Ref<godot::Font> _mono;
};

}
