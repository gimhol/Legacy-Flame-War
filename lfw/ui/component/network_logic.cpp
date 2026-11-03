#include "lfw/ui/component/network_logic.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/ui/component/text_input.h"
#include "lfw/ui/ui_event.h"
#include "lfw/ui/uinode.h"
#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

namespace {

// 房间行 / 玩家行的子节点下标，与 `ui/network_page.ui.xml` 的顺序一致
enum RoomChild : int {
  kRoomName = 0,
  kRoomCode = 1,
  kRoomBadgeBg = 2,
  kRoomLockBg = 3,
  kRoomPwdBg = 4,
  kRoomJoin = 5,
  kRoomOwner = 6,
  kRoomMeta = 7,
};

enum PlayerChild : int {
  kPlayerName = 0,
  kPlayerTags = 1,
  kPlayerRtt = 2,
  kPlayerState = 3,
  kPlayerKick = 4,
};

constexpr int kRoomRows = 5;
constexpr int kPlayerRows = 4;
constexpr int kChatRows = 15;
/** 新消息到达后消息列表保持完全不透明的时间，之后开始淡出（`update(dt)` 的 dt 是毫秒） */
constexpr double kChatHoldMs = 4000.0;
/** 淡出时长：这段时间内 alpha 从 1 线性降到 0 */
constexpr double kChatFadeMs = 800.0;
/** 按 T / 回车唤出聊天后，面板保持可点的时间 */
constexpr double kChatSummonMs = 8000.0;

std::u16string num_text(int v) { return number_to_string(static_cast<double>(v)); }

UINode* child_at(UINode& node, int index) {
  const std::vector<UINode*>& kids = node.children();
  return index >= 0 && index < static_cast<int>(kids.size()) ? kids[static_cast<size_t>(index)] : nullptr;
}

void set_child_text(UINode& node, int index, const std::u16string& text) {
  if (UINode* const child = child_at(node, index)) child->set_text(text);
}

void set_child_visible(UINode& node, int index, bool visible) {
  if (UINode* const child = child_at(node, index)) child->set_visible(visible);
}

}  // namespace

struct NetworkLogic::DPtr {
  explicit DPtr(NetworkLogic& owner) : self(owner) {}

  enum class Mode { kConnect, kRooms, kRoom };
  enum class AccountState { kOut, kPending, kIn };

  struct RoomRow {
    std::u16string title;
    int clients = 0;
    int max = 8;
    bool locked = false;
    bool started = false;
  };

  struct Seat {
    std::u16string name;
    bool ready = false;
    int rtt = 0;
    bool me = false;
  };

  using Handler = void (DPtr::*)();

  UINode* q(const char16_t* id);
  void collect(UINode& node, const std::u16string& id, std::vector<UINode*>& out);
  std::vector<UINode*> qa(const char16_t* id);
  std::u16string self_name() const { return nickname(); }
  std::u16string text_of(const char16_t* id) const;
  ui::TextInput* input_of(const char16_t* id) const;
  bool input_focused(const char16_t* id) const;
  std::u16string nickname() const;
  void bind(const char16_t* id, Handler fn);
  void bind_clicks();

  void set_status(const std::u16string& text);
  void set_mode(Mode next);
  void apply_mode();
  void apply_chat_visibility();
  void open_chat();
  void close_chat();
  void tick_chat(double dt);
  void apply_tab();
  void apply_account();
  void render_rooms();
  void render_players();
  void render_chat();
  void push_sys(const std::u16string& text);
  void refresh_stub_rooms();
  void on_key(LFWKeyEvent& e);

  void on_connect();
  void on_disconnect();
  void on_create();
  void on_refresh();
  void on_ready();
  void on_leave();
  void on_back_lobby();
  void on_start_game();
  void on_prev_page();
  void on_next_page();
  void on_send_chat();
  void on_chat_target();
  void on_tab_guest();
  void on_tab_account();
  void on_account_login();
  void on_account_mock();
  void on_account_logout();
  void on_account_enter();
  void join_room(int index);

  NetworkLogic& self;
  Mode mode = Mode::kConnect;
  AccountState account_state = AccountState::kOut;
  bool account_tab = false;
  bool connected = false;
  bool ready = false;
  int room_index = -1;
  int page = 0;
  int chat_target = 0;
  std::vector<std::u16string> chat;
  /** 用户按下聊天键、正在打字 */
  /** 新消息后还要完全不透明显示多久（秒） */
  double chat_hold = 0.0;
  /** 当前不透明度：1 = 完整显示，0 = 收起 */
  double chat_alpha = 0.0;
  std::vector<RoomRow> rooms;
  std::vector<Seat> seats;
};

UINode* NetworkLogic::DPtr::q(const char16_t* id) {
  return self.node.search_node(std::u16string(id));
}

void NetworkLogic::DPtr::collect(UINode& node, const std::u16string& id,
                                 std::vector<UINode*>& out) {
  if (to_string(node.id()) == id) out.push_back(&node);
  for (UINode* const child : node.children()) {
    if (child != nullptr) collect(*child, id, out);
  }
}

std::vector<UINode*> NetworkLogic::DPtr::qa(const char16_t* id) {
  std::vector<UINode*> out;
  collect(self.node, std::u16string(id), out);
  return out;
}

void NetworkLogic::DPtr::bind(const char16_t* id, Handler fn) {
  UINode* const node = q(id);
  if (node == nullptr) return;
  node->callbacks.on_click = [this, fn](LFWPointerEvent&) { (this->*fn)(); };
}

void NetworkLogic::DPtr::bind_clicks() {
  bind(u"network_btn_connect", &DPtr::on_connect);
  bind(u"network_btn_connect_cancel", &DPtr::on_disconnect);
  bind(u"network_btn_disconnect", &DPtr::on_disconnect);
  bind(u"network_btn_create", &DPtr::on_create);
  bind(u"network_btn_refresh", &DPtr::on_refresh);
  bind(u"network_btn_room_refresh", &DPtr::on_refresh);
  bind(u"network_btn_ready", &DPtr::on_ready);
  bind(u"network_btn_leave", &DPtr::on_leave);
  bind(u"network_btn_leave_top", &DPtr::on_leave);
  bind(u"network_btn_back_lobby", &DPtr::on_back_lobby);
  bind(u"network_btn_continue_game", &DPtr::on_start_game);
  bind(u"network_btn_prev", &DPtr::on_prev_page);
  bind(u"network_btn_next", &DPtr::on_next_page);
  bind(u"network_btn_send", &DPtr::on_send_chat);
  bind(u"network_btn_chat_target", &DPtr::on_chat_target);
  bind(u"network_tab_guest", &DPtr::on_tab_guest);
  bind(u"network_tab_account", &DPtr::on_tab_account);
  bind(u"network_btn_account_login", &DPtr::on_account_login);
  bind(u"network_btn_account_mock", &DPtr::on_account_mock);
  bind(u"network_btn_account_logout", &DPtr::on_account_logout);
  bind(u"network_btn_account_enter", &DPtr::on_account_enter);

  if (UINode* const node = q(u"network_chat_input")) {
    node->callbacks.on_click = [this](LFWPointerEvent&) { open_chat(); };
  }

  const std::vector<UINode*> rows = qa(u"network_room_row");
  for (size_t i = 0; i < rows.size(); ++i) {
    UINode* const row = rows[i];
    row->callbacks.on_click = [this, index = static_cast<int>(i)](LFWPointerEvent&) {
      join_room(page * kRoomRows + index);
    };
  }
}

void NetworkLogic::DPtr::on_key(LFWKeyEvent& e) {
  if (!e.pressed) return;
  // 打字由宿主叠的 LineEdit 覆盖层接管（与 Web 的 DOM `<input>` 同理）：
  // 这里只负责「对局中唤出聊天」，不碰文本内容。
  if (!input_focused(u"network_chat_input") && (e.key == u"t" || e.key == u"enter")) {
    open_chat();
  }
}

void NetworkLogic::DPtr::set_status(const std::u16string& text) {
  if (UINode* const node = q(u"network_top_sub")) node->set_text(text);
}

void NetworkLogic::DPtr::set_mode(Mode next) {
  mode = next;
  apply_mode();
}

void NetworkLogic::DPtr::apply_mode() {
  if (UINode* const panel = q(u"network_connect_panel")) {
    panel->set_visible(mode == Mode::kConnect);
  }
  if (UINode* const panel = q(u"network_rooms_panel")) panel->set_visible(mode == Mode::kRooms);
  if (UINode* const panel = q(u"network_room_panel")) panel->set_visible(mode == Mode::kRoom);
  if (UINode* const panel = q(u"network_overlay_panel")) panel->set_visible(false);
  apply_chat_visibility();

  const bool rooms = mode == Mode::kRooms;
  const bool room = mode == Mode::kRoom;
  if (UINode* const node = q(u"network_btn_create")) node->set_visible(rooms);
  if (UINode* const node = q(u"network_btn_refresh")) node->set_visible(rooms);
  if (UINode* const node = q(u"network_btn_auto")) node->set_visible(rooms);
  if (UINode* const node = q(u"network_btn_disconnect")) node->set_visible(connected);
  if (UINode* const node = q(u"network_btn_room_refresh")) node->set_visible(room);
  if (UINode* const node = q(u"network_btn_leave_top")) node->set_visible(room);
  if (UINode* const node = q(u"network_btn_logout")) {
    node->set_visible(account_state == AccountState::kIn);
  }
  if (UINode* const node = q(u"network_account_chip_bg")) {
    node->set_visible(account_state == AccountState::kIn);
  }
  render_chat();
}

void NetworkLogic::DPtr::apply_chat_visibility() {
  UINode* const panel = q(u"network_chat_panel");
  if (panel == nullptr) return;
  const bool show = connected && chat_alpha > 0.01;
  panel->set_visible(show);
  panel->set_opacity(chat_alpha);
}

void NetworkLogic::DPtr::open_chat() {
  if (!connected) return;
  chat_hold = kChatSummonMs;
  chat_alpha = 1.0;
  apply_chat_visibility();
}

void NetworkLogic::DPtr::close_chat() {
  chat_hold = 0.0;
  apply_chat_visibility();
}

void NetworkLogic::DPtr::tick_chat(double dt) {
  const double before = chat_alpha;
  const bool typing = input_focused(u"network_chat_input");
  if (typing) {
    chat_hold = kChatSummonMs;
    chat_alpha = 1.0;
  } else if (chat_hold > 0.0) {
    chat_hold = std::max(0.0, chat_hold - dt);
    chat_alpha = 1.0;
  } else if (chat_alpha > 0.0) {
    chat_alpha = std::max(0.0, chat_alpha - dt / kChatFadeMs);
  }
  if (chat_alpha != before) apply_chat_visibility();
}

void NetworkLogic::DPtr::apply_tab() {  if (UINode* const body = q(u"network_guest_body")) body->set_visible(!account_tab);
  if (UINode* const body = q(u"network_account_body")) body->set_visible(account_tab);
  apply_account();
}

void NetworkLogic::DPtr::apply_account() {
  if (UINode* const node = q(u"network_account_out")) {
    node->set_visible(account_state == AccountState::kOut);
  }
  if (UINode* const node = q(u"network_account_pending")) {
    node->set_visible(account_state == AccountState::kPending);
  }
  if (UINode* const node = q(u"network_account_in")) {
    node->set_visible(account_state == AccountState::kIn);
  }
  if (UINode* const node = q(u"network_account_name")) {
    node->set_text(account_state == AccountState::kIn ? nickname() : u"-");
  }
  if (UINode* const node = q(u"network_account_id")) {
    node->set_text(account_state == AccountState::kIn ? u"gim_1024" : u"-");
  }
}

std::u16string NetworkLogic::DPtr::text_of(const char16_t* id) const {
  ui::TextInput* const input = input_of(id);
  return input != nullptr ? input->text() : std::u16string();
}

ui::TextInput* NetworkLogic::DPtr::input_of(const char16_t* id) const {
  UINode* const node = const_cast<NetworkLogic&>(self).node.search_node(std::u16string(id));
  if (node == nullptr) return nullptr;
  UIComponent* const comp = node->search_component(ui::TextInput::class_tag());
  if (comp == nullptr || !is_class(comp->clazz(), ui::TextInput::class_tag())) return nullptr;
  return static_cast<ui::TextInput*>(comp);
}

bool NetworkLogic::DPtr::input_focused(const char16_t* id) const {
  UINode* const node = const_cast<NetworkLogic&>(self).node.search_node(std::u16string(id));
  return node != nullptr && node->focused();
}

std::u16string NetworkLogic::DPtr::nickname() const {
  const std::u16string text = text_of(u"network_nickname_input");
  return text.empty() ? std::u16string(u"Gim") : text;
}

void NetworkLogic::DPtr::refresh_stub_rooms() {
  rooms.clear();
  rooms.push_back(RoomRow{u"Gim 的房间", 2, 8, false, false});
  rooms.push_back(RoomRow{u"路人局", 5, 8, false, false});
  rooms.push_back(RoomRow{u"带锁的房", 3, 8, true, false});
  rooms.push_back(RoomRow{u"深夜局", 7, 8, false, true});
  rooms.push_back(RoomRow{u"菜鸟互啄", 1, 8, false, false});
  rooms.push_back(RoomRow{u"弹幕房", 4, 8, false, false});
  rooms.push_back(RoomRow{u"4v4 固定队", 8, 8, false, true});
  rooms.push_back(RoomRow{u"测试房", 0, 8, false, false});
  page = 0;
  render_rooms();
}

void NetworkLogic::DPtr::render_rooms() {
  const std::vector<UINode*> rows = qa(u"network_room_row");
  const int total_pages = std::max(1, (static_cast<int>(rooms.size()) + kRoomRows - 1) / kRoomRows);
  page = std::min(std::max(0, page), total_pages - 1);
  for (size_t i = 0; i < rows.size(); ++i) {
    UINode& row = *rows[i];
    const int index = page * kRoomRows + static_cast<int>(i);
    const bool used = index < static_cast<int>(rooms.size());
    row.set_visible(used);
    if (!used) continue;
    const RoomRow& info = rooms[static_cast<size_t>(index)];
    set_child_text(row, kRoomName, info.title);
    set_child_text(row, kRoomCode, std::u16string(u"#") + std::u16string(1, static_cast<char16_t>(u'A' + index)));
    set_child_text(row, kRoomMeta, num_text(info.clients) + u"/" + num_text(info.max));
    set_child_text(row, kRoomOwner, info.clients > 0 ? u"房主在线" : u"空房");
    if (UINode* const badge_bg = child_at(row, kRoomBadgeBg)) {
      badge_bg->set_visible(true);
      if (UINode* const badge = child_at(*badge_bg, 0)) {
        badge->set_text(info.started ? u"游戏中" : u"等待");
      }
    }
    set_child_visible(row, kRoomLockBg, info.locked);
    set_child_visible(row, kRoomPwdBg, info.locked);
    set_child_visible(row, kRoomJoin, true);
  }
  if (UINode* const node = q(u"network_rooms_total")) {
    node->set_text(u"第 " + num_text(page + 1) + u"/" + num_text(total_pages) + u" 页 · 共 " +
                   num_text(static_cast<int>(rooms.size())) + u" 个");
  }
  if (UINode* const node = q(u"network_rooms_status")) {
    node->set_text(u"（桩数据：点击房间行加入）");
  }
}

void NetworkLogic::DPtr::render_players() {
  const std::vector<UINode*> rows = qa(u"network_player_row");
  for (size_t i = 0; i < rows.size(); ++i) {
    UINode& row = *rows[i];
    const bool used = i < seats.size() && i < static_cast<size_t>(kPlayerRows);
    row.set_visible(used);
    if (!used) continue;
    const Seat& seat = seats[i];
    set_child_text(row, kPlayerName, seat.me ? seat.name + u"（我）" : seat.name);
    set_child_text(row, kPlayerTags, seat.me ? u"本地" : u"远程");
    set_child_text(row, kPlayerRtt, u"rtt " + num_text(seat.rtt) + u"ms");
    set_child_text(row, kPlayerState, seat.ready ? u"已准备" : u"未准备");
    set_child_visible(row, kPlayerKick, !seat.me);
  }
  if (UINode* const empty = q(u"network_empty_slot")) {
    empty->set_visible(seats.size() < static_cast<size_t>(kPlayerRows));
  }
}

void NetworkLogic::DPtr::push_sys(const std::u16string& text) {
  chat.push_back(u"[系统] " + text);
  render_chat();
  if (!input_focused(u"network_chat_input")) {
    chat_hold = kChatHoldMs;
    chat_alpha = 1.0;
  }
  apply_chat_visibility();
}

void NetworkLogic::DPtr::render_chat() {
  const std::vector<UINode*> rows = qa(u"network_chat_row");
  const int from = std::max(0, static_cast<int>(chat.size()) - kChatRows);
  for (size_t i = 0; i < rows.size(); ++i) {
    UINode& row = *rows[i];
    const int index = from + static_cast<int>(i);
    const bool used = index < static_cast<int>(chat.size());
    row.set_visible(used);
    if (!used) continue;
    row.set_text(chat[static_cast<size_t>(index)]);
  }
  if (UINode* const node = q(u"network_chat_target")) {
    node->set_text(chat_target == 0 ? u"房间" : u"世界");
  }
}

void NetworkLogic::DPtr::join_room(int index) {
  if (index < 0 || index >= static_cast<int>(rooms.size())) return;
  const RoomRow& info = rooms[static_cast<size_t>(index)];
  if (info.started) {
    set_status(u"该房间已开局（桩数据）");
    return;
  }
  if (info.locked && text_of(u"network_room_pwd_input") != u"123") {
    set_status(u"密码错误（桩：123）");
    return;
  }
  room_index = index;
  ready = false;
  seats.clear();
  seats.push_back(Seat{nickname(), false, 12, true});
  for (int i = 1; i < std::min(info.clients, kPlayerRows); ++i) {
    seats.push_back(Seat{u"玩家" + num_text(i), i % 2 == 0, 20 + i * 15, false});
  }
  render_players();
  if (UINode* const title = q(u"network_room_title")) title->set_text(info.title);
  if (UINode* const code = q(u"network_room_code_label")) {
    code->set_text(std::u16string(u"#") + std::u16string(1, static_cast<char16_t>(u'A' + index)));
  }
  if (UINode* const count = q(u"network_room_count")) {
    count->set_text(num_text(info.clients) + u"/" + num_text(info.max));
  }
  set_status(u"已进入房间（桩数据）");
  push_sys(u"进入房间：" + info.title);
  set_mode(Mode::kRoom);
}

void NetworkLogic::DPtr::on_connect() {
  const std::u16string address = text_of(u"network_address_input");
  if (address.empty()) {
    set_status(u"先填服务器地址");
    return;
  }
  connected = true;
  ready = false;
  refresh_stub_rooms();
  set_status(u"已连接（桩数据）");
  push_sys(u"已连接 " + address);
  set_mode(Mode::kRooms);
}

void NetworkLogic::DPtr::on_disconnect() {
  connected = false;
  room_index = -1;
  seats.clear();
  chat.clear();
  chat_hold = 0.0;
  chat_alpha = 0.0;
  render_chat();
  set_status(u"未连接");
  set_mode(Mode::kConnect);
  apply_chat_visibility();
}

void NetworkLogic::DPtr::on_create() {
  rooms.insert(rooms.begin(), RoomRow{u"我的房间", 1, 8, false, false});
  page = 0;
  render_rooms();
  join_room(0);
  push_sys(u"已创建房间");
}

void NetworkLogic::DPtr::on_refresh() {
  if (!connected) return;
  refresh_stub_rooms();
  set_status(u"已刷新（桩数据）");
}

void NetworkLogic::DPtr::on_ready() {
  if (mode != Mode::kRoom || seats.empty()) return;
  ready = !ready;
  seats[0].ready = ready;
  render_players();
  set_status(ready ? u"已准备" : u"取消准备");
  push_sys(ready ? u"你已准备" : u"你取消了准备");
}

void NetworkLogic::DPtr::on_leave() {
  room_index = -1;
  ready = false;
  seats.clear();
  render_players();
  set_status(u"已离开房间");
  set_mode(Mode::kRooms);
}

void NetworkLogic::DPtr::on_back_lobby() {
  set_status(u"已返回大厅");
  set_mode(Mode::kRooms);
}

void NetworkLogic::DPtr::on_start_game() {
  if (!ready) {
    set_status(u"先准备再开局");
    return;
  }
  set_status(u"（桩）等待服务端开局…");
  push_sys(u"等待服务端开局（桩）");
}

void NetworkLogic::DPtr::on_prev_page() {
  if (page <= 0) return;
  --page;
  render_rooms();
}

void NetworkLogic::DPtr::on_next_page() {
  const int total_pages = std::max(1, (static_cast<int>(rooms.size()) + kRoomRows - 1) / kRoomRows);
  if (page + 1 >= total_pages) return;
  ++page;
  render_rooms();
}

void NetworkLogic::DPtr::on_send_chat() {
  ui::TextInput* const input = input_of(u"network_chat_input");
  const std::u16string text = input != nullptr ? input->text() : std::u16string();
  if (!text.empty()) {
    chat.push_back(nickname() + u"：" + text);
    render_chat();
  }
  if (input != nullptr) input->set_text(std::u16string());
  chat_hold = kChatHoldMs;
  chat_alpha = 1.0;
  apply_chat_visibility();
}

void NetworkLogic::DPtr::on_chat_target() {
  chat_target = (chat_target + 1) % 2;
  render_chat();
}

void NetworkLogic::DPtr::on_tab_guest() {
  account_tab = false;
  apply_tab();
}

void NetworkLogic::DPtr::on_tab_account() {
  account_tab = true;
  apply_tab();
}

void NetworkLogic::DPtr::on_account_login() {
  account_state = AccountState::kPending;
  apply_account();
  set_status(u"登录中（桩）");
}

void NetworkLogic::DPtr::on_account_mock() {
  account_state = AccountState::kIn;
  apply_account();
  set_status(u"已登录（桩）");
}

void NetworkLogic::DPtr::on_account_logout() {
  account_state = AccountState::kOut;
  apply_account();
  set_status(u"已退出账号（桩）");
}

void NetworkLogic::DPtr::on_account_enter() {
  on_connect();
}

const ClazzTag* NetworkLogic::class_tag() {
  static const ClazzTag tag{UIComponent::class_tag()};
  return &tag;
}

const std::vector<std::u16string>& NetworkLogic::TAGS() {
  static const std::vector<std::u16string> tags{u"NetworkLogic"};
  return tags;
}

NetworkLogic::NetworkLogic(UINode& layout, const std::u16string& f_name, const Value& info)
    : UIComponent(layout, f_name, info), dptr(std::make_unique<DPtr>(*this)) {}

NetworkLogic::~NetworkLogic() = default;

void NetworkLogic::on_start() {
  UIComponent::on_start();
  DPtr& d = *dptr;
  d.bind_clicks();
  d.set_status(u"未连接");
  d.set_mode(DPtr::Mode::kConnect);
  d.apply_tab();
  d.apply_account();
  d.render_chat();
}

void NetworkLogic::on_stop() { UIComponent::on_stop(); }

void NetworkLogic::update(double dt) { dptr->tick_chat(dt); }

void NetworkLogic::on_key_down(LFWKeyEvent& e) { dptr->on_key(e); }

}
}
