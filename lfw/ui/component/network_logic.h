#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/net/net_service.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class UINode;

/**
 * 联机大厅的逻辑：连接弹窗（`network_modal`）、房间列表（`network_rooms_page`）、
 * 房间内（`network_room_page`）、暂停/重连浮层（`network_overlay_page`）。
 *
 * 对齐 Web 端 `src/pages/network/NetworkLogic.ts`：
 * - 组件是单例（`SINGLETON = true`），挂在 `lfw.host_node` 上，各页面只做链接；
 * - 网络收发走 `lfw.net_service()`（宿主侧的 `NetClient`）；
 * - 聊天内容进 `chat_store`，由 `ChatLogic` 渲染。
 */
class NetworkLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "NetworkLogic";
  static constexpr bool SINGLETON = true;
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  NetworkLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~NetworkLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;
  void on_key_down(LFWKeyEvent& e) override;
  void on_link(UINode& node) override;
  void on_unlink(UINode& node) override;

 private:
  struct DPtr;
  const std::unique_ptr<DPtr> dptr;
};

}  // namespace ui
}  // namespace lfw
