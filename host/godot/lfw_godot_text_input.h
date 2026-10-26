#pragma once

#include <functional>
#include <string>
#include <vector>

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "host/godot/lfw_godot_renderer.h"

namespace lfw::host {

/**
 * UI 的文本输入框（`GodotTextInputBox`）→ 真 `LineEdit`。
 *
 * 每帧 `sync()` 把覆盖层搬到 UI 给的位置、套上 UI 给的字体 / 颜色 / 对齐，
 * 再把两边的文本对齐（用户在 `LineEdit` 里改了就写回 `ui::TextInput`，
 * 否则用 `ui::TextInput` 的值刷新 `LineEdit`）。`UINode::focused` 也跟着焦点走。
 */
class GodotTextInputs {
 public:
  struct Overlay {
    godot::LineEdit* edit = nullptr;
    lfw::ui::UINode* node = nullptr;
    std::u16string shown;
    std::u16string synced;
    std::string style_sig;
    bool had_focus = false;
  };

  using FontFn = std::function<godot::Ref<godot::Font>(const std::string& css)>;

  /** `to_canvas` 是逻辑像素 → 画布像素；`boxes` 为空就销毁全部覆盖层 */
  void sync(godot::Node& owner, const std::vector<GodotTextInputBox>& boxes,
            const godot::Transform2D& to_canvas, const FontFn& font_for_css);
  /** 卸掉全部覆盖层（换页 / 关窗口 / 退出） */
  void destroy();

  const std::vector<Overlay>& overlays() const { return _overlays; }
  Overlay* last() { return _overlays.empty() ? nullptr : &_overlays.back(); }

  /** 放开所有输入焦点（开控制台前，免得两边抢字） */
  void release_focus();
  /** 点击落在某个输入框外面 ⇒ 放掉它的焦点 */
  void release_focus_outside(const godot::Vector2& point);

 private:
  godot::Node* _owner = nullptr;
  std::vector<Overlay> _overlays;
};

}
