#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace lfw {

class LFW;

namespace net {

/** 聊天频道：房间里（含对局中）只能发房间频道，房间外只能发全局频道。 */
enum class ChatTarget { kGlobal, kRoom };

const char16_t* chat_target_name(ChatTarget target);

/** 一条聊天记录：系统提示，或一条真人消息。 */
struct ChatItem {
  bool sys = false;
  std::u16string sender_id;
  std::u16string sender_name;
  std::u16string text;
  ChatTarget target = ChatTarget::kGlobal;
};

/**
 * 聊天内容与发送通道（对齐 Web 端 `src/Net/chat_store.ts`）。
 *
 * 只做数据：浮层（`chat_overlay`）负责开关，`ChatLogic` 负责渲染，
 * 真正的网络发送由使用方通过 `sender` 挂进来。
 */
class ChatStore {
 public:
  using Sender = std::function<void(const std::u16string& text, ChatTarget target)>;

  const std::vector<ChatItem>& items() const { return _items; }
  ChatTarget target() const { return _target; }
  bool in_room() const { return _target == ChatTarget::kRoom; }
  void set_in_room(bool in_room) {
    _target = in_room ? ChatTarget::kRoom : ChatTarget::kGlobal;
  }

  void clear() {
    _items.clear();
    peek = false;
  }

  void set_sender(Sender fn) { _sender = std::move(fn); }
  bool has_sender() const { return static_cast<bool>(_sender); }

  void push(ChatItem item);
  void push_sys(const std::u16string& text);
  /** 发送：文本为空或没有发送通道时返回 `false`（对齐 TS 的 `send()`）。 */
  bool send(const std::u16string& text);

  /** `true` = 只读预览（收到消息后自动收起输入框）。 */
  bool peek = false;

 private:
  std::vector<ChatItem> _items;
  Sender _sender;
  ChatTarget _target = ChatTarget::kGlobal;
};

}  // namespace net
}  // namespace lfw
