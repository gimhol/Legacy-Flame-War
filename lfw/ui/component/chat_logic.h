#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/net/chat_store.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class TextInput;
class UINode;

/**
 * 聊天浮层（`ui/chat_page.ui.xml`）的逻辑：消息列表、输入框、预览淡出。
 *
 * 对齐 Web 端 `src/pages/network/ChatLogic.ts`：
 * - `T` 唤出 / `Esc` 退出输入由 `chat_overlay` 与宿主输入框处理；
 * - 收到新消息时以「预览态」停留 `PEEK_HOLD_MS`，再淡出关闭；
 * - 输入态下的消息列表贴着输入框上沿，预览态下移到最底部。
 */
class ChatLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "ChatLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  ChatLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~ChatLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;
  void on_key_down(LFWKeyEvent& e) override;

  void send();
  /** `Esc` 退出输入：收起输入框，列表像收到消息那样停留一段时间后再淡出关闭。 */
  void exit_input();
  void close();
  void focus_input();

 private:
  struct Row {
    UINode* name = nullptr;
    UINode* body = nullptr;
  };

  UINode* q(const char16_t* id);
  net::ChatStore& store();
  std::u16string channel_label(net::ChatTarget target);
  void set_input_visible(bool visible);
  void blur_input();
  void keep_focus();
  void render();
  double render_row(Row& row, const net::ChatItem& item);
  void layout();
  double measure(const std::u16string& text, UINode* node);
  std::u16string fit(const std::u16string& text, double max_w, UINode* node);
  std::u16string wrap_lines(const std::u16string& text, double max_w, UINode* node,
                            int max_lines);

  std::vector<Row> _rows;
  UINode* _list = nullptr;
  UINode* _input_node = nullptr;
  TextInput* _input = nullptr;
  double _hold = 0.0;
  double _alpha = 1.0;
  bool _closing = false;
  int _seen = -1;
  net::ChatTarget _target = net::ChatTarget::kGlobal;
  bool _target_set = false;
  bool _focus_pending = false;
  int _focus_frames = 0;
  bool _locked = false;
};

}  // namespace ui
}  // namespace lfw
