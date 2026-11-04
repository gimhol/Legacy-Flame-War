#pragma once

#include <memory>
#include <string>

#include "lfw/lfw.h"

namespace lfw {
namespace net {

/** 聊天浮层独占的 UI 层，压在其他页面之上但不挤掉任何页面。 */
constexpr double CHAT_LAYER = 2.0;

/** 聊天页是否已经打开。 */
bool is_chat_open(LFW& lfw);
/** 打开聊天页（进入输入态）。 */
void open_chat(LFW& lfw);
/** 以只读预览态打开聊天页（收到新消息时用）。 */
void peek_chat(LFW& lfw);
/** 关闭聊天页。 */
void close_chat(LFW& lfw);
/** T 键语义：没开就开；预览态切到输入态；输入态则关掉。 */
void toggle_chat(LFW& lfw);

/**
 * `T` 键唤出聊天（对齐 Web 端 `src/Net/chat_overlay.ts` 的 `install_chat_hotkey`）。
 *
 * 只在有发送通道（已连上服务器）时生效；命中后打断这次按键，避免继续传给游戏。
 */
class ChatHotkey : public LfwKeyboardCallback {
 public:
  explicit ChatHotkey(LFW& lfw) : _lfw(&lfw) {}
  void on_key_down(LfwKeyEvent& e) override;

 private:
  LFW* _lfw = nullptr;
};

/** 安装热键（重复调用无副作用）。 */
void install_chat_hotkey(LFW& lfw);
/** 卸载热键（`LFW` 销毁前）。 */
void uninstall_chat_hotkey(LFW& lfw);

}  // namespace net
}  // namespace lfw
