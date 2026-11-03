#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {
namespace ui {

class UINode;

/** 联机大厅页（`ui/network_page.ui.xml`）的逻辑：连接 / 房间列表 / 房间内 三态切换。
 *  当前用桩数据驱动纯 UI，接上 `host/net` 的传输后把桩换成真实会话即可。 */
class NetworkLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "NetworkLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();

  NetworkLogic(UINode& layout, const std::u16string& f_name, const Value& info);
  ~NetworkLogic() override;

  const ClazzTag* clazz() const override { return class_tag(); }

  void on_start() override;
  void on_stop() override;
  void update(double dt) override;
  void on_key_down(LFWKeyEvent& e) override;

 private:
  struct DPtr;

  const std::unique_ptr<DPtr> dptr;
};

}
}
