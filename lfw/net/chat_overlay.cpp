#include "lfw/net/chat_overlay.h"

#include <map>
#include <memory>
#include <utility>

#include "lfw/net/chat_store.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"

namespace lfw::net {

namespace {

std::map<LFW*, std::unique_ptr<ChatHotkey>>& hotkeys() {
  static std::map<LFW*, std::unique_ptr<ChatHotkey>> map;
  return map;
}

bool same_id(const Value& id, const char16_t* want) {
  const std::u16string* const s = std::get_if<std::u16string>(&id);
  return s != nullptr && *s == want;
}

Value chat_page_opts() {
  Object o;
  o.set(u"id", Value(std::u16string(u"chat")));
  return Value(std::make_shared<Object>(o));
}

}  // namespace

bool is_chat_open(LFW& lfw) {
  ui::UILayers* const layers = lfw.ui_layers();
  ui::UILayer* const layer = layers != nullptr ? layers->at(CHAT_LAYER) : nullptr;
  if (layer == nullptr) return false;
  for (ui::UINode* const page : layer->pages()) {
    if (page != nullptr && same_id(page->id(), u"chat")) return true;
  }
  return false;
}

void open_chat(LFW& lfw) {
  if (is_chat_open(lfw)) return;
  lfw.chat_store().peek = false;
  if (ui::UILayers* const layers = lfw.ui_layers()) {
    layers->push_page(chat_page_opts(), CHAT_LAYER);
  }
}

void peek_chat(LFW& lfw) {
  if (is_chat_open(lfw)) return;
  lfw.chat_store().peek = true;
  if (ui::UILayers* const layers = lfw.ui_layers()) {
    layers->push_page(chat_page_opts(), CHAT_LAYER);
  }
}

void close_chat(LFW& lfw) {
  if (!is_chat_open(lfw)) return;
  if (ui::UILayers* const layers = lfw.ui_layers()) {
    layers->pop_page(Value(std::make_shared<Object>()), CHAT_LAYER);
  }
}

void toggle_chat(LFW& lfw) {
  if (!is_chat_open(lfw)) {
    open_chat(lfw);
    return;
  }
  if (lfw.chat_store().peek) {
    close_chat(lfw);
    open_chat(lfw);
    return;
  }
  close_chat(lfw);
}

void ChatHotkey::on_key_down(LfwKeyEvent& e) {
  if (_lfw == nullptr) return;
  // 预览态（输入框已收起）按 Esc 关掉聊天：对齐 Web 端挂在 window 上的捕获监听。
  // 输入态由宿主拦在 `LineEdit` 那一层（`TextInput::on_cancel`），不会走到这里。
  if (e.key == u"escape" && is_chat_open(*_lfw)) {
    if (_lfw->chat_store().peek) {
      close_chat(*_lfw);
      e.interrupt();
    }
    return;
  }
  const bool is_t = e.key == u"t" || e.key == u"T";
  if (!is_t) return;
  ChatStore& store = _lfw->chat_store();
  if (store.peek || is_chat_open(*_lfw)) {
    toggle_chat(*_lfw);
    e.interrupt();
    return;
  }
  if (!store.has_sender()) return;
  open_chat(*_lfw);
  e.interrupt();
}

void install_chat_hotkey(LFW& lfw) {
  std::map<LFW*, std::unique_ptr<ChatHotkey>>& map = hotkeys();
  if (map.count(&lfw) != 0) return;
  std::unique_ptr<ChatHotkey> hotkey = std::make_unique<ChatHotkey>(lfw);
  lfw.keyboard_callback_add(hotkey.get());
  map.emplace(&lfw, std::move(hotkey));
}

void uninstall_chat_hotkey(LFW& lfw) {
  std::map<LFW*, std::unique_ptr<ChatHotkey>>& map = hotkeys();
  const std::map<LFW*, std::unique_ptr<ChatHotkey>>::iterator it = map.find(&lfw);
  if (it == map.end()) return;
  lfw.keyboard_callback_del(it->second.get());
  map.erase(it);
}

}  // namespace lfw::net
