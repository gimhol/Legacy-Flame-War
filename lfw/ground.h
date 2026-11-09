#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "lfw/defines/i_terrain_info.h"
#include "lfw/utils/math/line_plane_intersection.h"

namespace lfw {

struct BlockPoint {
  double x = 0.0;
  double z = 0.0;
};

struct GroundHit {
  Vec3 point{0.0, 0.0, 0.0};
  ITerrainInfo segment;
};

class Ground {
 public:
  static const ITerrainInfo& horizon();
  static const ITerrainInfo& abyss();

  /** 更新地形分段（来自当前背景的 `terrain`）。空表示只有地平线。 */
  void set_terrain(std::vector<ITerrainInfo> terrain) { _terrain = std::move(terrain); }

  double step() const { return _step; }
  void set_step(double v) { _step = v; }
  const ITerrainInfo& base() const { return horizon(); }

  /**
   * 取坐标点地形分段
   *
   * 传了 `y` 才会按“单向平台”规则跳过位于物体上方的平台（允许从下方穿过）。
   * `rising` = 物体正在上升（velocity.y > 0）：此时必须严格高过顶面才算它，否则会被平台吸住。
   * `prev_y` = 本帧移动前的高度：下落时若本帧从顶面上方跨过，仍算这块平台（防止高速下落穿透）。
   */
  const ITerrainInfo& segment(double x, double z, std::optional<double> y = std::nullopt,
                              bool rising = false,
                              std::optional<double> prev_y = std::nullopt) const;
  static double y(const ITerrainInfo& seg, double x, double z);
  std::optional<double> enterable(const ITerrainInfo& seg, double x, double y, double z) const;
  const std::vector<BlockPoint>& block(const ITerrainInfo& seg, double x, double y, double z,
                                       std::optional<double> prev_x = std::nullopt,
                                       std::optional<double> prev_y = std::nullopt,
                                       std::optional<double> prev_z = std::nullopt);
  GroundHit intersect(double x1, double y1, double z1, double x2, double y2, double z2);
  std::optional<BlockPoint> intersect_wall(double x1, double y1, double z1, double x2, double y2,
                                           double z2) const;

 private:
  std::vector<ITerrainInfo> _terrain;
  double _step = 10.0;
  std::vector<BlockPoint> _ret = std::vector<BlockPoint>(4);
  std::vector<BlockPoint> _empty;
  GroundHit _hit;
};

}
