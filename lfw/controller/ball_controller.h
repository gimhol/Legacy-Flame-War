#pragma once

#include <string>
#include <vector>

#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/defines/i_vector3.h"

namespace lfw {
namespace controller {

class BallController : public BaseController {
 public:
  static constexpr const char* TAG = "BallController";

  bool is_ball_ctrl() const override { return true; }

  Value frame;
  bool gave_up = false;
  double dir_x = 0;
  double dir_y = 0;
  double dir_z = 0;
  double leave_dir = 0;

  void reset() override;

  Vector3& chase_point();
  void set_chase_point(double x, double y, double z);
  void aim_at(const Entity& e, double oy = 0);

  void update_lookup(int me, const std::vector<Entity*>& entities);
  bool should_chase(const Entity* other) const;

  const ControllerResult& update() override;
  void update_chasing(const Value& chase);
  double calc_dir(double delta, double over, double prev);
  void stop_chasing();

 private:
  void copy_self_position();

  bool has_chase_point_ = false;
  Vector3 chase_point_;
};

}
}
