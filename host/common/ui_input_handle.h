#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/uinode.h"

namespace lfw {
class World;
}

namespace lfw::host {

struct UiScreenSize {
  double w = 794;
  double h = 450;
};
UiScreenSize ui_screen_size(const World& world);

struct UiPoint {
  double x = 0;
  double y = 0;
};
UiPoint ui_point_of_scene(const World& world, double scene_x, double scene_y);
void ui_scene_of_point(const World& world, double x, double y, double& scene_x, double& scene_y);

std::u16string ui_node_path(const ui::UINode& node);

class UiInputHandle : public LfwPointingsCallback {
 public:
  explicit UiInputHandle(LFW& lfw) : _lfw(&lfw) {}

  void on_click(LfwPointingEvent& e) override { (void)e; }
  void on_pointer_down(LfwPointingEvent& e) override;
  void on_pointer_move(LfwPointingEvent& e) override;
  void on_pointer_up(LfwPointingEvent& e) override;
  void on_pointer_cancel(LfwPointingEvent& e) override;
  void on_wheel(LfwPointingEvent& e) override { (void)e; }
  void on_pointer_enter(LfwPointingEvent& e);
  void on_pointer_leave(LfwPointingEvent& e);

  void set_ui_offset(double x, double y, double z);

 private:
  struct Hit {
    ui::UINode* node = nullptr;
    double x = 0;
    double y = 0;
    double z = 0;
    double order = 0;
  };

  std::u16string pos_arg(const LfwPointingEvent& e) const;
  double world_z(const ui::UINode& node) const;
  void update_local_cursor(const LfwPointingEvent& e, std::optional<bool> hidden,
                           std::optional<bool> down = std::nullopt);
  std::vector<Hit> intersections(const LfwPointingEvent& e, ui::UINode& page,
                                 std::vector<std::u16string>* visits = nullptr) const;
  void push_empty_move(const LfwPointingEvent& e);
  void push_pointer(const char16_t* command, const LfwPointingEvent& e);

  LFW* _lfw = nullptr;
  double _offset_x = 0;
  double _offset_y = 0;
  double _offset_z = 0;
  bool _empty_move = false;
};

}
