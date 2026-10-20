#include "host/thin/thin_host.h"

namespace lfw::host {

bool ThinHost::step_frame(lfw::World& world, double dt, int sub_render) {
  const int before = _recorder->render_count();
  world.update_once(dt);
  const int world_rendered = _recorder->render_count() - before;
  const int total = sub_render > 1 ? sub_render : 1;
  const double sub_dt = dt / total;
  for (int i = world_rendered; i < total; ++i) world.render_once(sub_dt);
  return world_rendered == 0;
}

}
