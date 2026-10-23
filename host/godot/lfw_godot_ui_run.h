#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <vector>

#include "host/godot/lfw_godot_renderer.h"

namespace lfw::host {

class LfwGodot;

/**
 * 一个「绘制段」= 同一裁剪矩形的一串绘制命令，画在自己的 canvas item 上。
 *
 * 整块 UI 平时都画在 `LfwGodot` 自己的 canvas item 上，而一个 canvas item 只有一个裁剪矩形；
 * 想让滚动视图像 TS 的 `overflow: hidden` 那样按像素截断，就得把被裁的那一段挪到独立
 * canvas item：`canvas_item_set_clip` + `custom_rect` 会裁掉这条 item **自己的**绘制命令
 *（在 `_draw` 里设置，和宿主裁剪游戏区时一样），z_index 按段序递增保证顺序不变。
 */
class LfwGodotUiRun : public godot::Node2D {
  GDCLASS(LfwGodotUiRun, godot::Node2D)

 public:
  /** `kind` 为这两个值的命令走宿主自己的绘制函数 */
  static constexpr const char* kConsoleKind = "__lfw_console";
  static constexpr const char* kTitlebarKind = "__lfw_titlebar";

  void set_owner_node(LfwGodot* v) { _owner = v; }

  /** 接管一段命令；`clip` 为真时把这一段裁到 `rect` */
  void assign(std::vector<GodotDrawItem> items, bool clip, const godot::Rect2& rect);
  /** 往当前这一段尾巴上追加命令（标题栏 / 控制台这种要压在 UI 之上的） */
  void append(std::vector<GodotDrawItem> items);
  void clear();

  void _draw() override;

 protected:
  static void _bind_methods() {}

 private:
  void apply_clip();

  LfwGodot* _owner = nullptr;
  std::vector<GodotDrawItem> _items;
  bool _clipped = false;
  godot::Rect2 _rect;
};

}
