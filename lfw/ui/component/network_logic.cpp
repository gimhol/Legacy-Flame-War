#include "lfw/ui/component/network_logic.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "lfw/core/js_num.h"
#include "lfw/defines/i_data_info.h"
#include "lfw/lfw.h"
#include "lfw/net/chat_overlay.h"
#include "lfw/net/chat_store.h"
#include "lfw/ui/component/text_input.h"
#include "lfw/ui/ui_event.h"
#include "lfw/ui/uilayer.h"
#include "lfw/ui/uinode.h"
#include "lfw/ui/value_spread.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/is_class.h"
#include "lfw/world.h"

namespace lfw {
namespace ui {

namespace {

constexpr double CONNECT_LAYER = 0.0;
constexpr double OVERLAY_LAYER = 1.0;
constexpr double ROOM_ROW_STEP = 56.0;
constexpr double PLAYER_ROW_STEP = 32.0;
constexpr double ROOM_ROWS = 5.0;
constexpr double PLAYER_ROWS = 4.0;
constexpr double AUTO_REFRESH_MS = 5000.0;
constexpr double COUNTDOWN_MS = 5000.0;
/** 建房时的默认人数（对齐 Web 端 `{ min_players: 1, max_players: 4 }`） */
constexpr double kCreateMinPlayers = 1.0;
constexpr double kCreateMaxPlayers = 4.0;

enum RoomChild : int { kRoomJoin = 5, kRoomPwdBg = 4 };
enum PlayerChild : int { kPlayerRtt = 2, kPlayerState = 3, kPlayerKick = 4 };

constexpr const char16_t* ROOMS_PAGE_ID = u"network_rooms_page";
constexpr const char16_t* ROOM_PAGE_ID = u"network_room_page";
constexpr const char16_t* CONNECT_PAGE_ID = u"network_modal";
constexpr const char16_t* OVERLAY_PAGE_ID = u"network_overlay_page";

std::u16string text_of(const Value& v) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr ? *s : std::u16string();
}

bool id_is(const Value& id, const char16_t* want) {
  const std::u16string* const s = std::get_if<std::u16string>(&id);
  return s != nullptr && *s == want;
}

UINode* child_at(UINode& node, int index) {
  const std::vector<UINode*>& kids = node.children();
  return index >= 0 && index < static_cast<int>(kids.size()) ? kids[static_cast<size_t>(index)]
                                                             : nullptr;
}

TextInput* input_of(UINode* node) {
  if (node == nullptr) return nullptr;
  UIComponent* const comp = node->find_component(TextInput::class_tag());
  if (comp == nullptr || !is_class(comp->clazz(), TextInput::class_tag())) return nullptr;
  return static_cast<TextInput*>(comp);
}

std::u16string trim(const std::u16string& text) {
  size_t begin = 0;
  size_t end = text.size();
  const auto space = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; };
  while (begin < end && space(text[begin])) ++begin;
  while (end > begin && space(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

std::u16string upper_first(const std::u16string& text) {
  if (text.empty()) return u"?";
  std::u16string out = text.substr(0, 1);
  if (out[0] >= u'a' && out[0] <= u'z') out[0] = static_cast<char16_t>(out[0] - u'a' + u'A');
  return out;
}

/** 对齐 Web 端 `NICKNAME_MAX_LENGTH`：按码点截断，避免把代理对切坏 */
constexpr size_t kNicknameMaxLength = 10;

std::u16string clamp_nickname(const std::u16string& name) {
  size_t count = 0;
  for (size_t i = 0; i < name.size(); ++i) {
    if (count == kNicknameMaxLength) return name.substr(0, i);
    const char16_t c = name[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < name.size()) ++i;
    ++count;
  }
  return name;
}

/** 对齐 Web 端 `pick_default_nickname`：形容词 + 名词，中文/英文两套 */
std::u16string pick_default_nickname(bool zh, double entropy) {
  static const char16_t* const kEnAdj[] = {
      u"Swift", u"Shadow", u"Iron",  u"Frost",  u"Blaze",   u"Silent", u"Golden",
      u"Crimson", u"Storm", u"Night", u"Thunder", u"Ghost",  u"Steel",  u"Lucky",
      u"Mystic", u"Savage", u"Turbo", u"Pixel",  u"Sleepy",  u"Scrappy", u"Zippy",
  };
  static const char16_t* const kEnNoun[] = {
      u"Fox",   u"Tiger",  u"Wolf",  u"Blade", u"Fist",   u"Panda",  u"Dragon",
      u"Falcon", u"Cobra", u"Shark", u"Otter", u"Penguin", u"Hamster", u"Dumpling",
      u"Ramen", u"Mochi",  u"Waffle", u"Potato", u"Latte", u"Nugget", u"Popcorn",
  };
  static const char16_t* const kZhAdj[] = {
      u"疾风", u"无敌", u"大力", u"孤独", u"神秘", u"疯狂", u"憨厚", u"霸气",
      u"呆萌", u"闪电", u"烈火", u"寒冰", u"幻影", u"铁血", u"摸鱼", u"星辰",
      u"麻辣", u"佛系", u"逍遥", u"旋风", u"中二",
  };
  static const char16_t* const kZhNoun[] = {
      u"拳王", u"豆腐", u"忍者", u"弓手", u"铁甲", u"大侠", u"剑客", u"菜鸟",
      u"火锅", u"辣条", u"包子", u"汤圆", u"咸鱼", u"熊猫", u"老虎", u"狗子",
      u"憨憨", u"少侠", u"扫地僧", u"米线", u"饺子",
  };
  const size_t adj_count = zh ? std::size(kZhAdj) : std::size(kEnAdj);
  const size_t noun_count = zh ? std::size(kZhNoun) : std::size(kEnNoun);
  const char16_t* const* const adjs = zh ? kZhAdj : kEnAdj;
  const char16_t* const* const nouns = zh ? kZhNoun : kEnNoun;
  // 昵称只是本地默认值，不吃 `mt`（那是联机的确定性随机源），用墙钟毫秒当熵。
  const auto rnd = [entropy](double salt, size_t n) {
    if (n == 0) return size_t{0};
    return static_cast<size_t>(std::fmod(std::fabs(salt), static_cast<double>(n)));
  };
  return std::u16string(adjs[rnd(entropy, adj_count)]) +
         nouns[rnd(entropy * 7.0 + 3.0, noun_count)];
}

}  // namespace

struct NetworkLogic::DPtr : public net::INetListener {
  explicit DPtr(NetworkLogic& owner) : self(owner) {}

  enum class Mode { kConnect, kRooms, kRoom, kStarted };

  NetworkLogic& self;
  Mode mode = Mode::kConnect;
  bool connecting = false;
  bool busy = false;
  std::u16string address;
  std::u16string status;
  double auto_timer = 0.0;
  double top_timer = 0.0;
  /** 各客户端上报的 rtt（key 是 client id；自己那条用实测值）。 */
  std::map<std::u16string, double> rtts;
  bool auto_refresh = true;
  int page = 0;
  std::vector<std::u16string> leavers;
  int reconnecting = 0;
  bool rejoin_failed = false;
  bool all_ready = false;
  double countdown_elapsed = 0.0;
  int countdown_sec = 5;
  bool countdown_sent = false;

  UINode* connect_page = nullptr;
  UINode* rooms_page = nullptr;
  UINode* room_page = nullptr;
  UINode* overlay_page = nullptr;
  /**
   * 当前仍然活着、且由本组件管理的页面根节点。
   *
   * 联机 UI 是**单例**（挂在 `host_node` 上），页面换了以后 `update`/网络回调
   * 仍会每帧跑。一旦某个页面的指针没清干净，写进去的就是已经析构的节点；那块
   * 内存可能被新页面（比如准备页的 `player_name`/`player_team` 格）重新用掉，
   * 于是别人的昵称就"长"在了角色选择框里（只在联机流程出现过）。
   * 所有写操作先过 `owned()`：节点得活着，且属于本组件管着的页面。
   */
  std::vector<const UINode*> page_roots;

  UINode* address_input = nullptr;
  UINode* nickname_input = nullptr;
  UINode* connect_status = nullptr;
  UINode* btn_connect = nullptr;
  UINode* btn_connect_cancel = nullptr;
  UINode* connect_close = nullptr;

  UINode* top_sub = nullptr;
  UINode* account_chip_bg = nullptr;
  UINode* account_chip = nullptr;
  UINode* btn_logout = nullptr;
  UINode* btn_create = nullptr;
  UINode* btn_refresh = nullptr;
  UINode* btn_auto = nullptr;
  UINode* btn_disconnect = nullptr;
  UINode* btn_room_refresh = nullptr;
  UINode* btn_leave_top = nullptr;

  UINode* rooms_total = nullptr;
  UINode* no_rooms = nullptr;
  UINode* btn_prev = nullptr;
  UINode* btn_next = nullptr;
  UINode* page_label = nullptr;
  UINode* rooms_status = nullptr;
  std::vector<UINode*> room_rows;

  UINode* room_title = nullptr;
  UINode* room_code = nullptr;
  UINode* room_badge = nullptr;
  UINode* room_badge_bg = nullptr;
  UINode* room_count = nullptr;
  UINode* sync_auto = nullptr;
  UINode* sync_lockstep = nullptr;
  UINode* sync_delay = nullptr;
  UINode* recommend = nullptr;
  UINode* room_lfw = nullptr;
  UINode* room_data_0 = nullptr;
  UINode* room_data_1 = nullptr;
  UINode* pwd_bg = nullptr;
  UINode* pwd_input = nullptr;
  UINode* btn_set_pwd = nullptr;
  UINode* pwd_hint = nullptr;
  UINode* empty_slot = nullptr;
  UINode* countdown = nullptr;
  UINode* btn_leave = nullptr;
  UINode* btn_ready = nullptr;
  UINode* room_status = nullptr;
  std::vector<UINode*> player_rows;

  UINode* overlay_panel = nullptr;
  UINode* overlay_title = nullptr;
  UINode* overlay_line_0 = nullptr;
  UINode* overlay_line_1 = nullptr;
  UINode* overlay_dim = nullptr;
  UINode* btn_continue_solo = nullptr;
  UINode* btn_back_lobby = nullptr;
  UINode* btn_continue_game = nullptr;
  /** 与 Web 端 `_syncing_page` / `_syncing_overlay` 一致：换页/开关浮层时会触发
   *  `on_link` → `bind_*` → 再次进来，没有这个闸就会递归。 */
  bool syncing_page = false;
  bool syncing_overlay = false;

  net::INetService* svc = nullptr;
  /** 组件刚创建、还没链接到任何页面：此时不能开弹窗（会递归创建第二个弹窗）。 */
  bool start_pending = false;

  void q(UINode& node, const char16_t* id, UINode*& out) { out = node.search_node(std::u16string(id)); }

  std::u16string t(const char16_t* key) {
    return text_of(self.lfw().string(Value(std::u16string(key))));
  }

  /** 把词条里的 `%1` 换成 `arg`（对齐 TS `t(key, ...args)`）。 */
  std::u16string t1(const char16_t* key, const std::u16string& arg) {
    std::u16string text = t(key);
    const size_t at = text.find(u"%1");
    if (at != std::u16string::npos) text.replace(at, 2, arg);
    return text;
  }

  /** 节点还活着，且挂在当前由本组件管理的页面里。 */
  bool owned(const UINode* node) const {
    if (node == nullptr || !is_live_uinode(node)) return false;
    const UINode* const root = &node->root();
    for (const UINode* const r : page_roots) {
      if (r == root) return true;
    }
    return false;
  }

  void set_text(UINode* node, const std::u16string& text) {
    if (!owned(node)) return;
    if (equal_text(node, text)) return;
    node->set_text(text);
  }

  bool equal_text(UINode* node, const std::u16string& text) {
    const Value* const txt = field_of(node->text(), u"text");
    const std::u16string* const s = txt != nullptr ? std::get_if<std::u16string>(txt) : nullptr;
    return s != nullptr && *s == text;
  }

  void set_status(const std::u16string& text) {
    status = text;
    set_text(connect_status, text);
    sync_status_visibility();
  }

  void clear_status() { set_status(std::u16string()); }

  void sync_status_visibility() {
    for (UINode* const node : {connect_status, rooms_status}) {
      if (!owned(node)) continue;
      node->set_visible(!status.empty());
    }
  }

  void set_input(UINode* node, const std::u16string& value) {
    if (!owned(node)) return;
    TextInput* const input = input_of(node);
    if (input != nullptr) input->set_text(value);
  }

  std::u16string input_value(UINode* node) {
    TextInput* const input = input_of(node);
    return input != nullptr ? input->text() : std::u16string();
  }

  std::u16string nickname() const { return self.lfw().net_service() != nullptr
                                            ? self.lfw().net_service()->nickname()
                                            : std::u16string(); }

  bool is_owner() {
    if (svc == nullptr) return false;
    const std::optional<net::NetRoomInfo>& room = svc->room();
    const std::optional<net::NetClientInfo>& me = svc->client();
    return room.has_value() && me.has_value() && room->owner.info.id == me->id;
  }

  void sync_pages(Mode next) {
    if (syncing_page) return;
    ui::UILayers* const layers = self.lfw().ui_layers();
    if (layers == nullptr) return;
    if (next == Mode::kStarted) return;
    const char16_t* const want =
        next == Mode::kRoom ? ROOM_PAGE_ID : (next == Mode::kRooms ? ROOMS_PAGE_ID : nullptr);
    ui::UILayer* const layer = layers->at(CONNECT_LAYER);
    const Value cur_id = layer != nullptr && layer->page() != nullptr ? layer->page()->id() : Value();
    syncing_page = true;
    if (want != nullptr) {
      if (!id_is(cur_id, want)) {
        Object opts;
        opts.set(u"id", Value(std::u16string(want)));
        layers->set_page(Value(std::make_shared<Object>(opts)), CONNECT_LAYER);
      }
      if (connect_page != nullptr) {
        Object opts;
        opts.set(u"id", Value(std::u16string(CONNECT_PAGE_ID)));
        layers->close_modal(Value(std::make_shared<Object>(opts)), CONNECT_LAYER);
      }
    } else {
      if (id_is(cur_id, ROOMS_PAGE_ID) || id_is(cur_id, ROOM_PAGE_ID)) {
        Object opts;
        opts.set(u"id", Value(std::u16string(u"entry")));
        layers->set_page(Value(std::make_shared<Object>(opts)), CONNECT_LAYER);
      }
      if (connect_page == nullptr) {
        Object opts;
        opts.set(u"id", Value(std::u16string(CONNECT_PAGE_ID)));
        layers->open_modal(Value(std::make_shared<Object>(opts)), CONNECT_LAYER);
      }
    }
    syncing_page = false;
  }

  /** 暂停/重连浮层单独一层，盖在游戏之上（对齐 Web 端 `sync_overlay_page`）。 */
  void sync_overlay_page(bool show) {
    if (syncing_overlay) return;
    if (show == (overlay_page != nullptr)) return;
    ui::UILayers* const layers = self.lfw().ui_layers();
    if (layers == nullptr) return;
    syncing_overlay = true;
    if (show) {
      Object opts;
      opts.set(u"id", Value(std::u16string(OVERLAY_PAGE_ID)));
      layers->push_page(Value(std::make_shared<Object>(opts)), OVERLAY_LAYER);
    } else {
      Object opts;
      opts.set(u"min_pages", Value(0.0));
      layers->pop_page(Value(std::make_shared<Object>(opts)), OVERLAY_LAYER);
    }
    syncing_overlay = false;
  }

  void set_mode(Mode next) {
    const Mode prev = mode;
    mode = next;
    apply_mode();
    if (next != Mode::kStarted) sync_pages(next);
    if (next == Mode::kRooms && prev != Mode::kRooms) {
      auto_timer = 0.0;
      request_rooms();
    }
    self.lfw().chat_store().set_in_room(next == Mode::kRoom || next == Mode::kStarted);
    if (next == Mode::kStarted) blur_inputs();
    update_topbar();
    render_rooms();
    render_room();
    update_overlay();
  }

  void apply_mode() {
    if (UINode* const n = connect_page) {
      if (UINode* const panel = n->search_node(u"network_connect_panel")) {
        panel->set_visible(mode == Mode::kConnect);
      }
    }
    const bool rooms = mode == Mode::kRooms;
    const bool room = mode == Mode::kRoom || mode == Mode::kStarted;
    if (owned(btn_create)) btn_create->set_visible(rooms);
    if (owned(btn_refresh)) btn_refresh->set_visible(rooms);
    if (owned(btn_auto)) btn_auto->set_visible(rooms);
    if (owned(btn_disconnect)) btn_disconnect->set_visible(svc != nullptr && svc->opened());
    if (owned(btn_room_refresh)) btn_room_refresh->set_visible(room);
    if (owned(btn_leave_top)) btn_leave_top->set_visible(room);
    sync_status_visibility();
  }

  void bind_click(UINode* node, void (DPtr::*fn)()) {
    if (node == nullptr) return;
    node->callbacks.on_click = [this, fn](LFWPointerEvent&) { (this->*fn)(); };
  }

  void bind_page(UINode& node) {
    const bool known = id_is(node.id(), CONNECT_PAGE_ID) || id_is(node.id(), ROOMS_PAGE_ID) ||
                       id_is(node.id(), ROOM_PAGE_ID) || id_is(node.id(), OVERLAY_PAGE_ID);
    if (!known) return;
    // 先登记成"本组件管着的页面"，bind_xxx 里会立刻写节点（`owned()` 需要它已就位）
    bool registered = false;
    for (const UINode* const r : page_roots) {
      if (r == &node) {
        registered = true;
        break;
      }
    }
    if (!registered) page_roots.push_back(&node);
    if (id_is(node.id(), CONNECT_PAGE_ID)) bind_connect_page(node);
    else if (id_is(node.id(), ROOMS_PAGE_ID)) bind_rooms_page(node);
    else if (id_is(node.id(), ROOM_PAGE_ID)) bind_room_page(node);
    else bind_overlay_page(node);
    if (start_pending) {
      // 第一次链接到页面（= 用户点了「联机」）才初始化模式；在 on_start 里做会递归开弹窗
      start_pending = false;
      set_mode(Mode::kConnect);
    } else {
      apply_mode();
      update_topbar();
      render_rooms();
      render_room();
      update_overlay();
    }
  }

  /**
   * 与 Web 端 `on_unlink` 一一对应。**保留下面的字段清单是为了能跟 TS 直接对比**，
   * 真正保证不悬垂的是 `forget_page_nodes()` —— C++ 没有 GC，TS 里"忘清"的字段只是
   * 指向一个已脱离页面的对象（写进去没人看得见），在这里就是野指针。
   */
  void unbind_page(UINode& node) {
    for (size_t i = 0; i < page_roots.size(); ++i) {
      if (page_roots[i] != &node) continue;
      page_roots.erase(page_roots.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
    if (id_is(node.id(), CONNECT_PAGE_ID)) unbind_connect_page(node);
    else if (id_is(node.id(), ROOMS_PAGE_ID)) unbind_rooms_page(node);
    else if (id_is(node.id(), ROOM_PAGE_ID)) unbind_room_page(node);
    else if (id_is(node.id(), OVERLAY_PAGE_ID)) unbind_overlay_page(node);
    forget_page_nodes(node);
  }

  /** 对齐 TS `on_unlink` 里的 `case CONNECT_PAGE_ID: this.connect_page = void 0`。 */
  void unbind_connect_page(UINode& node) {
    if (connect_page != &node) return;
    connect_page = nullptr;
    address_input = nickname_input = connect_status = nullptr;
    btn_connect = btn_connect_cancel = connect_close = nullptr;
  }

  /** 对齐 TS `unbind_rooms_page`。 */
  void unbind_rooms_page(UINode& node) {
    if (rooms_page != &node) return;
    rooms_page = nullptr;
    top_sub = account_chip_bg = account_chip = btn_logout = nullptr;
    btn_create = btn_refresh = btn_auto = btn_disconnect = nullptr;
    rooms_total = no_rooms = btn_prev = btn_next = page_label = rooms_status = nullptr;
    room_rows.clear();
  }

  /** 对齐 TS `unbind_room_page`。 */
  void unbind_room_page(UINode& node) {
    if (room_page != &node) return;
    room_page = nullptr;
    top_sub = account_chip_bg = account_chip = btn_logout = nullptr;
    btn_room_refresh = btn_leave_top = nullptr;
    room_title = room_code = room_badge = room_badge_bg = room_count = nullptr;
    sync_auto = sync_lockstep = sync_delay = recommend = nullptr;
    room_lfw = room_data_0 = room_data_1 = nullptr;
    pwd_bg = pwd_input = btn_set_pwd = pwd_hint = nullptr;
    empty_slot = countdown = btn_leave = btn_ready = room_status = nullptr;
    player_rows.clear();
  }

  /** 对齐 TS `unbind_overlay_page`。 */
  void unbind_overlay_page(UINode& node) {
    if (overlay_page != &node) return;
    overlay_page = nullptr;
    overlay_dim = overlay_panel = overlay_title = nullptr;
    overlay_line_0 = overlay_line_1 = nullptr;
    btn_continue_solo = btn_back_lobby = btn_continue_game = nullptr;
  }

  /**
   * 把还指向 `page` 的所有成员指针置空。
   *
   * `network_top_sub` / `network_account_chip` 等节点在**房间页和房间列表页里都有**，
   * 以前只按页面 id 各清各的，漏掉的那一组会留在已经析构的节点上：本组件是单例，
   * `update_topbar()` 每帧照写，写的是别人的内存 —— 那块内存随后被新页面（准备页）
   * 复用，昵称就"长"在了角色选择格里。所以这里统一按"节点属于哪个页面"清。
   */
  void forget_page_nodes(const UINode& page) {
    const auto forget = [&page](UINode*& p) {
      if (p == nullptr) return;
      if (!is_live_uinode(p) || &p->root() == &page) p = nullptr;
    };
    const auto forget_all = [&forget](std::vector<UINode*>& rows) {
      for (UINode*& p : rows) forget(p);
    };
    forget(address_input);
    forget(nickname_input);
    forget(connect_status);
    forget(btn_connect);
    forget(btn_connect_cancel);
    forget(connect_close);
    forget(overlay_panel);
    forget(overlay_title);
    forget(overlay_line_0);
    forget(overlay_line_1);
    forget(btn_continue_solo);
    forget(btn_back_lobby);
    forget(btn_continue_game);
    forget(top_sub);
    forget(account_chip_bg);
    forget(account_chip);
    forget(btn_logout);
    forget(btn_create);
    forget(btn_refresh);
    forget(btn_auto);
    forget(btn_disconnect);
    forget(btn_room_refresh);
    forget(btn_leave_top);
    forget(rooms_total);
    forget(no_rooms);
    forget(btn_prev);
    forget(btn_next);
    forget(page_label);
    forget(rooms_status);
    forget(room_title);
    forget(room_code);
    forget(room_badge);
    forget(room_badge_bg);
    forget(room_count);
    forget(sync_auto);
    forget(sync_lockstep);
    forget(sync_delay);
    forget(recommend);
    forget(room_lfw);
    forget(room_data_0);
    forget(room_data_1);
    forget(pwd_bg);
    forget(pwd_input);
    forget(btn_set_pwd);
    forget(pwd_hint);
    forget(empty_slot);
    forget(countdown);
    forget(btn_leave);
    forget(btn_ready);
    forget(room_status);
    forget_all(room_rows);
    forget_all(player_rows);
  }

  void bind_connect_page(UINode& node) {
    connect_page = &node;
    q(node, u"network_address_input", address_input);
    q(node, u"network_nickname_input", nickname_input);
    q(node, u"network_connect_status", connect_status);
    q(node, u"network_btn_connect", btn_connect);
    q(node, u"network_btn_connect_cancel", btn_connect_cancel);
    q(node, u"network_connect_close", connect_close);
    bind_overlay_nodes(node);

    bind_click(connect_close, &DPtr::on_close_page);
    bind_click(btn_connect, &DPtr::on_connect);
    bind_click(btn_connect_cancel, &DPtr::on_cancel_connect);
    bind_click(btn_continue_solo, &DPtr::on_continue_solo);
    bind_click(btn_back_lobby, &DPtr::on_back_lobby);
    bind_click(btn_continue_game, &DPtr::on_continue_game);

    if (TextInput* const input = input_of(address_input)) {
      input->on_submit = [this]() { on_connect(); };
    }
    if (TextInput* const input = input_of(nickname_input)) {
      input->on_submit = [this]() { on_connect(); };
    }
    if (address_input != nullptr && input_value(address_input).empty()) {
      set_input(address_input, u"lfj.gim.ink");
    }
    if (nickname_input != nullptr && input_value(nickname_input).empty()) {
      set_input(nickname_input, default_nickname());
    }
    if (connect_page != nullptr) {
      if (UINode* const panel = connect_page->search_node(u"network_connect_panel")) {
        panel->set_visible(mode == Mode::kConnect);
      }
    }
    sync_status_visibility();
  }

  std::u16string default_nickname() {
    const Value lang = self.lfw().canonical_lang();
    const std::u16string* const code = std::get_if<std::u16string>(&lang);
    return pick_default_nickname(code != nullptr && code->size() >= 2 &&
                                     (*code)[0] == u'z' && (*code)[1] == u'h',
                                 self.lfw().host().now());
  }

  void bind_rooms_page(UINode& node) {
    rooms_page = &node;
    q(node, u"network_top_sub", top_sub);
    q(node, u"network_account_chip_bg", account_chip_bg);
    q(node, u"network_account_chip", account_chip);
    q(node, u"network_btn_logout", btn_logout);
    q(node, u"network_btn_create", btn_create);
    q(node, u"network_btn_refresh", btn_refresh);
    q(node, u"network_btn_auto", btn_auto);
    q(node, u"network_btn_disconnect", btn_disconnect);
    q(node, u"network_rooms_total", rooms_total);
    q(node, u"network_no_rooms", no_rooms);
    q(node, u"network_btn_prev", btn_prev);
    q(node, u"network_btn_next", btn_next);
    q(node, u"network_page_label", page_label);
    q(node, u"network_rooms_status", rooms_status);

    room_rows.clear();
    if (UINode* const rows = node.search_node(u"network_room_rows")) {
      room_rows = rows->children();
    }
    for (size_t i = 0; i < room_rows.size(); ++i) {
      UINode* const row = room_rows[i];
      if (row == nullptr) continue;
      row->move_to(0.0, static_cast<double>(i) * ROOM_ROW_STEP, 0.0);
      if (UINode* const join = child_at(*row, kRoomJoin)) {
        join->callbacks.on_click = [this, i](LFWPointerEvent&) { join_room(static_cast<int>(i)); };
      }
      if (UINode* const pwd_bg = child_at(*row, kRoomPwdBg)) {
        if (TextInput* const input = input_of(child_at(*pwd_bg, 0))) {
          input->on_submit = [this, i]() { join_room(static_cast<int>(i)); };
        }
      }
    }

    bind_click(btn_logout, &DPtr::on_account_logout);
    bind_click(btn_create, &DPtr::on_create);
    bind_click(btn_refresh, &DPtr::on_refresh);
    bind_click(btn_auto, &DPtr::on_toggle_auto);
    bind_click(btn_disconnect, &DPtr::on_disconnect);
    bind_click(btn_prev, &DPtr::on_prev_page);
    bind_click(btn_next, &DPtr::on_next_page);

    apply_mode();
    update_topbar();
    render_rooms();
  }

  void bind_room_page(UINode& node) {
    room_page = &node;
    q(node, u"network_top_sub", top_sub);
    q(node, u"network_account_chip_bg", account_chip_bg);
    q(node, u"network_account_chip", account_chip);
    q(node, u"network_btn_logout", btn_logout);
    q(node, u"network_btn_room_refresh", btn_room_refresh);
    q(node, u"network_btn_leave_top", btn_leave_top);
    q(node, u"network_room_title", room_title);
    q(node, u"network_room_code_label", room_code);
    q(node, u"network_room_panel_badge", room_badge);
    q(node, u"network_room_panel_badge_bg", room_badge_bg);
    q(node, u"network_room_count", room_count);
    q(node, u"network_sync_auto", sync_auto);
    q(node, u"network_sync_lockstep", sync_lockstep);
    q(node, u"network_sync_delay", sync_delay);
    q(node, u"network_recommend", recommend);
    q(node, u"network_room_lfw", room_lfw);
    q(node, u"network_room_data_0", room_data_0);
    q(node, u"network_room_data_1", room_data_1);
    q(node, u"network_pwd_bg", pwd_bg);
    q(node, u"network_pwd_input", pwd_input);
    q(node, u"network_btn_set_pwd", btn_set_pwd);
    q(node, u"network_pwd_hint", pwd_hint);
    q(node, u"network_empty_slot", empty_slot);
    q(node, u"network_countdown", countdown);
    q(node, u"network_btn_leave", btn_leave);
    q(node, u"network_btn_ready", btn_ready);
    q(node, u"network_room_status", room_status);

    player_rows.clear();
    if (UINode* const rows = node.search_node(u"network_player_rows")) {
      player_rows = rows->children();
    }
    for (size_t i = 0; i < player_rows.size(); ++i) {
      UINode* const row = player_rows[i];
      if (row == nullptr) continue;
      row->move_to(0.0, static_cast<double>(i) * PLAYER_ROW_STEP, 0.0);
      if (UINode* const kick = child_at(*row, kPlayerKick)) {
        kick->callbacks.on_click = [this, i](LFWPointerEvent&) { kick_row(static_cast<int>(i)); };
      }
    }

    bind_click(btn_logout, &DPtr::on_account_logout);
    bind_click(btn_room_refresh, &DPtr::on_room_refresh);
    bind_click(btn_leave_top, &DPtr::on_leave);
    bind_click(btn_leave, &DPtr::on_leave);
    bind_click(btn_ready, &DPtr::on_ready);
    bind_click(btn_set_pwd, &DPtr::on_set_pwd);
    bind_click(sync_auto, &DPtr::on_sync_auto);
    bind_click(sync_lockstep, &DPtr::on_sync_lockstep);
    bind_click(sync_delay, &DPtr::on_sync_delay);

    if (TextInput* const input = input_of(pwd_input)) {
      input->on_submit = [this]() { on_set_pwd(); };
    }

    apply_mode();
    update_topbar();
    render_room();
  }

  void bind_overlay_page(UINode& node) {
    overlay_page = &node;
    bind_overlay_nodes(node);
    bind_click(btn_continue_solo, &DPtr::on_continue_solo);
    bind_click(btn_back_lobby, &DPtr::on_back_lobby);
    bind_click(btn_continue_game, &DPtr::on_continue_game);
    update_overlay();
  }

  void bind_overlay_nodes(UINode& node) {
    q(node, u"network_overlay_dim", overlay_dim);
    q(node, u"network_overlay_panel", overlay_panel);
    q(node, u"network_overlay_title", overlay_title);
    q(node, u"network_overlay_line_0", overlay_line_0);
    q(node, u"network_overlay_line_1", overlay_line_1);
    q(node, u"network_btn_continue_solo", btn_continue_solo);
    q(node, u"network_btn_back_lobby", btn_back_lobby);
    q(node, u"network_btn_continue_game", btn_continue_game);
  }

  void update_overlay() {
    const bool show = reconnecting > 0 || rejoin_failed;
    sync_overlay_page(show);
    if (owned(overlay_dim)) overlay_dim->set_visible(show);
    if (owned(overlay_panel)) overlay_panel->set_visible(show);
    if (!show) return;
    set_text(overlay_title,
             rejoin_failed ? t(u"network.reconnect_failed") : t(u"network.reconnecting"));
    set_text(overlay_line_0,
             rejoin_failed ? t(u"network.reconnect_failed_hint")
                           : t1(u"network.reconnect_attempt", number_to_string(reconnecting)));
    set_text(overlay_line_1, std::u16string());
    // 对齐 Web 端：重连中/失败给「继续单机 / 返回大厅」；「继续游戏」只在掉线暂停时给房主，
    // 而暂停（`driver.suspend()`）还没做（里程碑 M4），所以这里和 TS 的 paused 分支一并留空。
    if (owned(btn_continue_solo)) btn_continue_solo->set_visible(show);
    if (owned(btn_back_lobby)) btn_back_lobby->set_visible(show);
    if (owned(btn_continue_game)) btn_continue_game->set_visible(false);
  }

  void update_topbar() {
    if (owned(top_sub)) {
      if (!status.empty()) {
        top_sub->set_text(status);
      } else if (svc != nullptr && svc->opened()) {
        // 对齐 Web 端：`地址 · Nms`
        const std::u16string rtt_text = number_to_string(js_round(svc->rtt())) + u"ms";
        top_sub->set_text(address.empty() ? rtt_text : address + u" · " + rtt_text);
      } else {
        top_sub->set_text(std::u16string());
      }
    }
    const bool chip = svc != nullptr && svc->opened() && svc->client().has_value();
    if (owned(account_chip_bg)) account_chip_bg->set_visible(chip);
    if (owned(btn_logout)) btn_logout->set_visible(false);
    if (owned(account_chip)) {
      account_chip->set_text(chip ? svc->client()->name : std::u16string());
    }
  }

  std::vector<net::NetRoomInfo> page_rooms() {
    std::vector<net::NetRoomInfo> out;
    if (svc == nullptr) return out;
    const size_t begin = static_cast<size_t>(std::max(0, page) * static_cast<int>(ROOM_ROWS));
    for (size_t i = begin; i < svc->rooms().size() && out.size() < static_cast<size_t>(ROOM_ROWS);
         ++i) {
      out.push_back(svc->rooms()[i]);
    }
    return out;
  }

  std::u16string room_count_text(const net::NetRoomInfo& room) {
    return number_to_string(static_cast<double>(room.clients.size())) + u"/" +
           number_to_string(static_cast<double>(room.max_players));
  }

  std::u16string sync_label(const std::u16string& mode) {
    if (mode == u"lockstep") return t(u"network.sync_lockstep");
    if (mode == u"delay") return t(u"network.sync_delay");
    return t(u"network.sync_auto");
  }

  void render_rooms() {
    if (svc == nullptr || rooms_page == nullptr) return;
    const std::vector<net::NetRoomInfo> rows = page_rooms();
    for (size_t i = 0; i < room_rows.size(); ++i) {
      UINode* const row = room_rows[i];
      if (row == nullptr) continue;
      if (i >= rows.size()) {
        row->set_visible(false);
        continue;
      }
      const net::NetRoomInfo& room = rows[i];
      row->set_visible(true);
      set_text(child_at(*row, 0), room.title);
      set_text(child_at(*row, 1), room.code);
      set_text(child_at(*row, 2), sync_label(room.sync_mode));
      set_text(child_at(*row, 6), room.owner.info.name);
      set_text(child_at(*row, 7), room_count_text(room));
      if (UINode* const lock_bg = child_at(*row, 3)) lock_bg->set_visible(room.need_pwd);
      if (UINode* const pwd_bg = child_at(*row, kRoomPwdBg)) {
        pwd_bg->set_visible(room.need_pwd);
        if (UINode* const input = child_at(*pwd_bg, 0)) input->set_visible(room.need_pwd);
      }
    }
    if (no_rooms != nullptr) no_rooms->set_visible(rows.empty());
    if (rooms_total != nullptr) {
      rooms_total->set_text(u"(" + number_to_string(static_cast<double>(svc->rooms().size())) + u")");
    }
    const int pages = static_cast<int>(
        std::ceil(static_cast<double>(svc->rooms().size()) / ROOM_ROWS));
    if (page_label != nullptr) {
      page_label->set_text(number_to_string(static_cast<double>(page + 1)) + u"/" +
                          number_to_string(static_cast<double>(std::max(1, pages))));
    }
    if (btn_prev != nullptr) btn_prev->set_visible(page > 0);
    if (btn_next != nullptr) btn_next->set_visible(page + 1 < pages);
    sync_status_visibility();
  }

  void render_room() {
    if (svc == nullptr || room_page == nullptr || !svc->room().has_value()) return;
    const net::NetRoomInfo& room = *svc->room();
    set_text(room_title, room.title);
    set_text(room_code, room.code);
    set_text(room_badge, sync_label(room.sync_mode));
    if (room_badge_bg != nullptr) room_badge_bg->set_visible(true);
    if (room_count != nullptr) room_count->set_text(room_count_text(room));
    if (room_lfw != nullptr) {
      const std::u16string short_version =
          room.lfw_version.substr(0, room.lfw_version.find(u' '));
      room_lfw->set_visible(!room.lfw_version.empty());
      set_text(room_lfw, room.lfw_version.empty()
                             ? std::u16string()
                             : t(u"network.lfw_version") + u": " + short_version);
    }
    const auto data_line = [this](const net::NetRoomInfo& r, size_t index) -> std::u16string {
      if (r.data_infos.size() <= index) return std::u16string();
      const net::NetDataInfo& info = r.data_infos[index];
      std::u16string text = t(u"network.data_package") + u"[" +
                            number_to_string(static_cast<double>(index + 1)) + u"] " +
                            info.type + info.title + u" v" + number_to_string(info.version);
      if (!info.md5.empty()) text += u" · " + info.md5.substr(0, 8);
      return text;
    };
    if (room_data_0 != nullptr) {
      room_data_0->set_visible(!room.data_infos.empty());
      set_text(room_data_0, data_line(room, 0));
    }
    if (room_data_1 != nullptr) {
      room_data_1->set_visible(room.data_infos.size() > 1);
      set_text(room_data_1, data_line(room, 1));
    }
    set_sync_style(sync_auto, room.sync_mode == u"auto");
    set_sync_style(sync_lockstep, room.sync_mode == u"lockstep");
    set_sync_style(sync_delay, room.sync_mode == u"delay");
    render_recommend();

    const bool owner = is_owner();
    if (pwd_bg != nullptr) pwd_bg->set_visible(owner);
    if (pwd_input != nullptr) pwd_input->set_visible(owner);
    if (btn_set_pwd != nullptr) btn_set_pwd->set_visible(owner);
    if (pwd_hint != nullptr) pwd_hint->set_visible(!owner);

    render_room_players();
    render_countdown();

    if (btn_ready != nullptr) {
      bool ready = false;
      if (svc->client().has_value()) {
        for (const net::NetRoomClient& c : room.clients) {
          if (c.info.id == svc->client()->id) ready = c.ready;
        }
      }
      btn_ready->set_text(ready ? t(u"network.cancel_ready") : t(u"network.ready"));
    }
    sync_status_visibility();
  }

  void set_sync_style(UINode* node, bool active) {
    if (node == nullptr) return;
    if (!active) return;
  }

  void render_room_players() {
    if (svc == nullptr || room_page == nullptr || !svc->room().has_value()) return;
    const net::NetRoomInfo& room = *svc->room();
    const std::u16string my_id = svc->client().has_value() ? svc->client()->id : std::u16string();
    for (size_t i = 0; i < player_rows.size(); ++i) {
      UINode* const row = player_rows[i];
      if (row == nullptr) continue;
      if (i >= room.clients.size()) {
        row->set_visible(false);
        continue;
      }
      const net::NetRoomClient& c = room.clients[i];
      row->set_visible(true);
      set_text(child_at(*row, 0), c.info.name.empty() ? c.info.id : c.info.name);
      std::u16string tags;
      if (!my_id.empty() && c.info.id == my_id) tags = u"(" + t(u"network.you") + u")";
      if (c.info.id == room.owner.info.id) {
        if (!tags.empty()) tags += u" ";
        tags += u"(" + t(u"network.owner") + u")";
      }
      set_text(child_at(*row, 1), tags);
      const auto rtt_it = rtts.find(c.info.id);
      const double rtt = rtt_it != rtts.end() ? rtt_it->second : 0.0;
      set_text(child_at(*row, kPlayerRtt),
               rtt > 0.0 ? number_to_string(js_round(rtt)) + u"ms" : std::u16string(u"-"));
      set_text(child_at(*row, kPlayerState),
               c.ready ? t(u"network.ready_done") : t(u"network.not_ready"));
      if (UINode* const kick = child_at(*row, kPlayerKick)) {
        const bool can_kick = is_owner() && !my_id.empty() && c.info.id != my_id;
        kick->set_visible(can_kick);
      }
    }
    if (empty_slot != nullptr) {
      empty_slot->set_visible(room.clients.size() < static_cast<size_t>(room.max_players));
    }
  }

  /**
   * 对齐 Web 端 `render_recommend`：按最差 rtt 给出推荐的同步模式。
   * 公式与 `host/net/net_sync.h` 的 `recommend_sync` 一致（core 不能依赖 host，
   * 所以这里只做展示用的同名计算；真正选模式仍走 host 的 `resolve_sync`）。
   */
  void render_recommend() {
    if (recommend == nullptr || svc == nullptr || !svc->room().has_value()) return;
    double worst = 0.0;
    for (const net::NetRoomClient& c : svc->room()->clients) {
      const auto it = rtts.find(c.info.id);
      if (it != rtts.end()) worst = std::max(worst, it->second);
    }
    if (!(worst > 0.0)) {
      set_text(recommend, std::u16string());
      return;
    }
    constexpr double kTickMs = 1000.0 / 60.0;
    constexpr double kLockstepRatio = 1.25;
    constexpr double kJitter = 1.3;
    constexpr int kMinLead = 2;
    constexpr int kMaxLead = 8;
    std::u16string label;
    if (worst <= kTickMs * kLockstepRatio) {
      label = t(u"network.sync_lockstep");
    } else {
      const int lead = std::clamp(
          static_cast<int>(std::ceil(worst * kJitter / kTickMs)), kMinLead, kMaxLead);
      label = t(u"network.sync_delay") + u" K=" + number_to_string(static_cast<double>(lead));
    }
    set_text(recommend, t(u"network.recommend") + u": " + label + u" (" +
                            number_to_string(js_round(worst)) + u"ms)");
  }

  void render_countdown() {
    if (countdown == nullptr) return;
    if (!all_ready || mode != Mode::kRoom) {
      countdown->set_visible(false);
      return;
    }
    countdown->set_visible(true);
    // 对齐 Web 端：读秒结束后换成「正在进入游戏…」
    countdown->set_text(countdown_sec > 0
                            ? t1(u"network.countdown",
                                 number_to_string(static_cast<double>(countdown_sec)))
                            : t(u"network.enter_game"));
  }

  void add_notice(const std::u16string& text) { self.lfw().chat_store().push_sys(text); }

  void add_chat(const Value& msg) {
    net::ChatItem item;
    item.text = text_of(field_or(msg, u"text"));
    const Value sender = field_or(msg, u"sender");
    if (as_object(sender) != nullptr) {
      item.sender_id = text_of(field_or(sender, u"id"));
      item.sender_name = text_of(field_or(sender, u"name"));
    }
    const std::u16string target = text_of(field_or(msg, u"target"));
    item.target = target == u"room" ? net::ChatTarget::kRoom : net::ChatTarget::kGlobal;
    if (svc != nullptr && svc->client().has_value() && item.sender_id == svc->client()->id) {
      self.lfw().chat_store().push(item);
      return;
    }
    if (svc != nullptr && svc->room().has_value() && item.target == net::ChatTarget::kGlobal) {
      self.lfw().chat_store().push(item);
      return;
    }
    self.lfw().chat_store().push(item);
    net::peek_chat(self.lfw());
  }

  void blur_inputs() {
    for (UINode* const node : {address_input, nickname_input, pwd_input}) {
      if (!owned(node)) continue;
      node->blur();
      if (TextInput* const input = input_of(node)) input->request_blur();
    }
  }

  // ---- net::INetListener ----

  void on_net_open() override { self.lfw().host().log({Value(u"net: open")}); }

  void on_net_close(const std::u16string& reason) override {
    (void)reason;
    const bool was_connecting = connecting;
    reset_connection();
    // 连都没连上（地址写错 / 服务器证书过期 / 网络不通）时给一句明确的失败提示，
    // 否则页面只是默默退回去，用户只能看到引擎层报错。
    if (was_connecting) set_status(t(u"network.connect_failed"));
  }

  void on_net_room(const net::NetRoomInfo* room) override {
    if (room == nullptr) {
      all_ready = false;
      countdown_elapsed = 0.0;
      countdown_sec = 5;
      countdown_sent = false;
      // `ClientInfo` 的 job 回调是「先落 `client` 再 `emit_room()`」，所以这里能判断出
      // 「已经进服」：把「连接中…」清掉，别让顶栏一直挂着它。
      if (svc != nullptr && svc->opened() && svc->client().has_value()) {
        set_connecting(false);
        clear_status();
      }
      update_overlay();
      set_mode(svc != nullptr && svc->opened() ? Mode::kRooms : Mode::kConnect);
      if (svc != nullptr && svc->opened()) request_rooms();
      return;
    }
    bool ready_all = !room->clients.empty() &&
                     room->clients.size() >= static_cast<size_t>(room->min_players);
    for (const net::NetRoomClient& c : room->clients) ready_all = ready_all && c.ready;
    if (ready_all != all_ready) {
      all_ready = ready_all;
      countdown_elapsed = 0.0;
      countdown_sec = 5;
      countdown_sent = false;
    }
    if (mode != Mode::kRoom && mode != Mode::kStarted) set_mode(Mode::kRoom);
    render_room();
    if (rooms_page != nullptr) render_rooms();
  }

  void on_net_rooms(const std::vector<net::NetRoomInfo>& rooms) override {
    (void)rooms;
    if (page > 0 && page_rooms().empty()) page = 0;
    if (rooms_page != nullptr) render_rooms();
  }

  void on_net_ping(const std::u16string& client_id, double rtt) override {
    // 记下每个人的 rtt：房间里每行显示，顶栏显示自己的，推荐同步模式用最差的
    if (client_id.empty() || !(rtt > 0.0)) {
      update_topbar();
      return;
    }
    const auto it = rtts.find(client_id);
    if (it != rtts.end() && it->second == rtt) {
      update_topbar();
      return;
    }
    rtts[client_id] = rtt;
    render_room_players();
    render_recommend();
    update_topbar();
  }

  void on_net_reconnecting(int attempt) override {
    reconnecting = attempt;
    rejoin_failed = false;
    update_overlay();
    self.lfw().chat_store().push_sys(t(u"network.reconnecting"));
  }

  void on_net_rejoin(const Value& resp) override {
    (void)resp;
    reconnecting = 0;
    rejoin_failed = false;
    update_overlay();
    self.lfw().chat_store().push_sys(t(u"network.sys_resumed"));
  }

  void on_net_rejoin_failed(bool rejected) override {
    (void)rejected;
    reconnecting = 0;
    rejoin_failed = true;
    update_overlay();
    self.lfw().chat_store().push_sys(t(u"network.reconnect_failed"));
  }

  void on_net_error(const std::u16string& text) override { set_status(text); }

  void on_net_message(const Value& resp) override {
    const std::u16string type = text_of(field_or(resp, u"type"));
    if (type == u"Chat") {
      add_chat(resp);
      return;
    }
    if (type == u"ClientInfo" && svc != nullptr && svc->client().has_value()) {
      // 进服成功（也可能是别人改了名字）：连上以后要把「连接中…」的状态清掉，
      // 顶栏再刷一遍——`ClientInfo` 有时比 `on_net_room(nullptr)` 的换页更晚到。
      const bool was_connecting = connecting;
      set_connecting(false);
      clear_status();
      update_topbar();
      if (was_connecting) add_notice(t1(u"network.sys_connected", address));
      if (mode == Mode::kConnect) set_mode(Mode::kRooms);
      return;
    }
    if (type == u"Kick") {
      const std::u16string name = text_of(field_or(field_or(resp, u"client"), u"name"));
      if (!name.empty()) add_notice(t1(u"network.sys_kicked", name));
    }
  }

  // ---- 页面动作 ----

  void on_connect() {
    if (svc == nullptr || svc->opened() || connecting) return;
    const std::u16string addr =
        trim(input_value(address_input)).empty() ? u"lfj.gim.ink" : trim(input_value(address_input));
    std::u16string nick = trim(input_value(nickname_input));
    if (nick.empty()) nick = default_nickname();
    nick = clamp_nickname(nick);
    if (nick.empty()) return;
    svc->set_nickname(nick);
    svc->set_players({});
    address = addr;
    clear_status();
    set_connecting(true);
    set_status(t(u"network.connecting"));
    svc->open(addr);
  }

  void set_connecting(bool value) {
    connecting = value;
    if (btn_connect != nullptr) btn_connect->set_visible(!value);
    if (btn_connect_cancel != nullptr) btn_connect_cancel->set_visible(value);
  }

  void on_cancel_connect() {
    if (connecting) {
      on_disconnect();
      return;
    }
    on_close_page();
  }

  void on_close_page() {
    on_disconnect();
    ui::UILayers* const layers = self.lfw().ui_layers();
    if (layers == nullptr) return;
    Object opts;
    opts.set(u"id", Value(std::u16string(CONNECT_PAGE_ID)));
    layers->close_modal(Value(std::make_shared<Object>(opts)), CONNECT_LAYER);
  }

  void on_disconnect() {
    if (svc != nullptr) {
      svc->disable_rejoin();
      svc->close();
    }
    reset_connection();
  }

  void reset_connection() {
    if (svc != nullptr) svc->disable_rejoin();
    reconnecting = 0;
    rejoin_failed = false;
    all_ready = false;
    countdown_elapsed = 0.0;
    countdown_sec = 5;
    countdown_sent = false;
    rtts.clear();
    set_connecting(false);
    clear_status();
    set_mode(Mode::kConnect);
    update_overlay();
  }

  void on_refresh() {
    page = 0;
    request_rooms();
  }

  void request_rooms() {
    if (svc == nullptr || !svc->opened()) return;
    svc->request(u"ListRooms", Value(), 3000.0, nullptr,
                 [this](const std::u16string& error) { set_status(error); });
  }

  void on_toggle_auto() {
    auto_refresh = !auto_refresh;
    auto_timer = 0.0;
    if (input_of(btn_auto) != nullptr) {
    }
  }

  void on_prev_page() {
    if (page <= 0) return;
    page -= 1;
    render_rooms();
  }

  void on_next_page() {
    page += 1;
    if (page_rooms().empty()) page -= 1;
    render_rooms();
  }

  /** 建房 / 加房都要带上版本与数据包指纹：服务端按 `lfw_version` + `data_infos[].md5` 校验（对齐 Web 端） */
  std::shared_ptr<Object> version_payload() {
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"lfw_version", Value(LFW::VERSION_NAME()));
    auto infos = std::make_shared<Array>();
    for (const IDataInfo& info : LFW::collect_data_infos()) {
      auto item = std::make_shared<Object>();
      item->set(u"type", info.type);
      item->set(u"url", info.url);
      item->set(u"title", info.title);
      item->set(u"description", info.description);
      item->set(u"author", info.author);
      item->set(u"version", info.version);
      item->set(u"time", info.time);
      item->set(u"md5", info.md5);
      infos->push_back(Value(item));
    }
    payload->set(u"data_infos", Value(infos));
    return payload;
  }

  void on_create() {
    if (svc == nullptr || !svc->opened() || busy) return;
    busy = true;
    std::shared_ptr<Object> payload = version_payload();
    payload->set(u"min_players", Value(kCreateMinPlayers));
    payload->set(u"max_players", Value(kCreateMaxPlayers));
    svc->request(u"CreateRoom", Value(payload), 5000.0,
                 [this](const Value&) {
                   busy = false;
                   set_mode(Mode::kRoom);
                 },
                 [this](const std::u16string& error) {
                   busy = false;
                   set_status(error);
                 });
  }

  void join_room(int index) {
    if (svc == nullptr || !svc->opened() || busy) return;
    const std::vector<net::NetRoomInfo> rows = page_rooms();
    if (index < 0 || static_cast<size_t>(index) >= rows.size()) return;
    const net::NetRoomInfo& room = rows[static_cast<size_t>(index)];
    std::u16string pwd;
    if (index < static_cast<int>(room_rows.size()) && room_rows[static_cast<size_t>(index)] != nullptr) {
      if (UINode* const bg = child_at(*room_rows[static_cast<size_t>(index)], kRoomPwdBg)) {
        pwd = trim(input_value(child_at(*bg, 0)));
      }
    }
    std::shared_ptr<Object> payload = version_payload();
    payload->set(u"roomid", room.id);
    if (!pwd.empty()) payload->set(u"pwd", Value(pwd));
    busy = true;
    svc->request(u"JoinRoom", Value(payload), 5000.0,
                 [this](const Value&) {
                   busy = false;
                   set_mode(Mode::kRoom);
                 },
                 [this](const std::u16string& error) {
                   busy = false;
                   set_status(error);
                 });
  }

  void on_leave() {
    if (svc == nullptr) return;
    svc->request(u"ExitRoom", Value(), 3000.0,
                 [this](const Value&) { set_mode(Mode::kRooms); },
                 [this](const std::u16string& error) { set_status(error); });
  }

  void on_ready() {
    if (svc == nullptr || !svc->room().has_value() || !svc->client().has_value()) return;
    bool ready = false;
    for (const net::NetRoomClient& c : svc->room()->clients) {
      if (c.info.id == svc->client()->id) ready = c.ready;
    }
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"ready", Value(!ready));
    svc->request(u"ClientReady", Value(payload), 3000.0, nullptr,
                 [this](const std::u16string& error) { set_status(error); });
  }

  void set_sync(const char16_t* mode) {
    if (svc == nullptr || !is_owner()) return;
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"sync_mode", Value(std::u16string(mode)));
    svc->request(u"RoomSync", Value(payload), 3000.0, nullptr,
                 [this](const std::u16string& error) { set_status(error); });
  }

  void on_sync_auto() { set_sync(u"auto"); }
  void on_sync_lockstep() { set_sync(u"lockstep"); }
  void on_sync_delay() { set_sync(u"delay"); }

  void on_set_pwd() {
    if (svc == nullptr || !is_owner()) return;
    const std::u16string pwd = trim(input_value(pwd_input));
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"pwd", Value(pwd));
    svc->request(u"RoomPwd", Value(payload), 3000.0, nullptr,
                 [this](const std::u16string& error) { set_status(error); });
  }

  void kick_row(int index) {
    if (svc == nullptr || !is_owner() || !svc->room().has_value()) return;
    const net::NetRoomInfo& room = *svc->room();
    if (index < 0 || static_cast<size_t>(index) >= room.clients.size()) return;
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"client_id", Value(room.clients[static_cast<size_t>(index)].info.id));
    svc->request(u"Kick", Value(payload), 3000.0, nullptr,
                 [this](const std::u16string& error) { set_status(error); });
  }

  void on_room_refresh() { render_room(); }

  void on_account_logout() { set_account_state_out(); }

  void set_account_state_out() { update_topbar(); }

  void on_continue_solo() {
    rejoin_failed = false;
    update_overlay();
    if (svc != nullptr) svc->close();
  }

  void on_back_lobby() {
    rejoin_failed = false;
    update_overlay();
    if (svc != nullptr) {
      svc->disable_rejoin();
      svc->close();
    }
    set_mode(Mode::kConnect);
  }

  void on_continue_game() {
    reconnecting = 0;
    update_overlay();
  }

  void tick(double dt) {
    top_timer += dt;
    if (top_timer >= 0.5) {
      top_timer = 0.0;
      if (mode == Mode::kRooms || mode == Mode::kRoom || mode == Mode::kStarted) update_topbar();
    }
    if (mode == Mode::kRooms && auto_refresh && svc != nullptr && svc->opened()) {
      auto_timer += dt;
      if (auto_timer >= AUTO_REFRESH_MS) {
        auto_timer = 0.0;
        request_rooms();
      }
    }
    if (mode != Mode::kRoom || !all_ready) return;
    countdown_elapsed += dt;
    const int sec = static_cast<int>(
        std::max(0.0, std::ceil((COUNTDOWN_MS - countdown_elapsed) / 1000.0)));
    if (sec != countdown_sec) {
      countdown_sec = sec;
      render_countdown();
    }
    if (sec > 0 || countdown_sent) return;
    countdown_sent = true;
    if (is_owner() && svc != nullptr) {
      svc->request(u"RoomStart", Value(), 5000.0, nullptr,
                   [this](const std::u16string& error) { set_status(error); });
    }
  }
};

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
  dptr->svc = lfw().net_service();
  if (dptr->svc != nullptr) {
    dptr->svc->add_listener(dptr.get());
    dptr->svc->enable_rejoin();
  }
  net::ChatStore& chat = lfw().chat_store();
  net::INetService* const svc = dptr->svc;
  chat.set_sender([svc](const std::u16string& text, net::ChatTarget target) {
    if (svc == nullptr) return;
    std::shared_ptr<Object> payload = std::make_shared<Object>();
    payload->set(u"target", Value(std::u16string(net::chat_target_name(target))));
    payload->set(u"text", Value(text));
    svc->send_nowait(u"Chat", Value(payload));
  });
  // 注意：这里**不要** `set_mode(kConnect)` —— 单例是在页面 `UINode::create` 期间创建的，
  // 此时 `open_modal` 会把同一个弹窗再创建一遍。等 `on_link` 拿到页面后再初始化。
  dptr->start_pending = true;
}

void NetworkLogic::on_stop() {
  if (dptr->svc != nullptr) {
    dptr->svc->remove_listener(dptr.get());
    dptr->svc->disable_rejoin();
    dptr->svc->close();
  }
  lfw().chat_store().set_sender(nullptr);
}

void NetworkLogic::update(double dt) { dptr->tick(dt); }

void NetworkLogic::on_key_down(LFWKeyEvent& e) {
  if (e.key != u"escape") return;
  if (dptr->mode == DPtr::Mode::kConnect) {
    dptr->on_close_page();
    e.stop_immediate_propagation();
  }
}

void NetworkLogic::on_link(UINode& node) { dptr->bind_page(node); }

void NetworkLogic::on_unlink(UINode& node) { dptr->unbind_page(node); }

}  // namespace ui
}  // namespace lfw
