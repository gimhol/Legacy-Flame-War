#include "lfw/net/chat_store.h"

#include <cstddef>
#include <utility>

namespace lfw::net {

namespace {

constexpr size_t kMaxItems = 100;

std::u16string trim(const std::u16string& text) {
  size_t begin = 0;
  size_t end = text.size();
  const auto space = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; };
  while (begin < end && space(text[begin])) ++begin;
  while (end > begin && space(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

}  // namespace

const char16_t* chat_target_name(ChatTarget target) {
  return target == ChatTarget::kRoom ? u"room" : u"global";
}

void ChatStore::push(ChatItem item) {
  _items.push_back(std::move(item));
  if (_items.size() > kMaxItems) {
    _items.erase(_items.begin(),
                 _items.begin() + static_cast<std::ptrdiff_t>(_items.size() - kMaxItems));
  }
}

void ChatStore::push_sys(const std::u16string& text) {
  ChatItem item;
  item.sys = true;
  item.text = text;
  push(std::move(item));
}

bool ChatStore::send(const std::u16string& text) {
  const std::u16string trimmed = trim(text);
  if (trimmed.empty() || !_sender) return false;
  _sender(trimmed, _target);
  return true;
}

}  // namespace lfw::net
