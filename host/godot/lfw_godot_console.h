#pragma once

#include <functional>
#include <string>
#include <vector>

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
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

  /** 追加日志（`kind` 见 `Line`）；`\n` 会拆成多行 */
  void append(int kind, const std::string& text);
  /** 执行一行命令（回显 + 记历史 + 交给 `ExecFn`）；空行忽略，`clear`/`cls` 清屏 */
  void execute(const std::string& line);
  /** LineEdit 回车回调：执行并清空输入 */
  void submit(const std::string& line);
  /** 滚动条拖动 / 点击的回调 */
  void on_scrollbar_value(double value);

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
};

}
