#pragma once

#include <memory>
#include <vector>

#include "lfw/defines/i_terrain_info.h"
#include "lfw/terrain/terrain_reachability.h"
#include "lfw/terrain/terrain_solid.h"

namespace lfw {
namespace terrain {

struct SurfaceQuery {
  bool has_y = false;
  double y = 0.0;
  bool rising = false;
  bool has_prev_y = false;
  double prev_y = 0.0;
  bool drop_through = false;
  double step = 10.0;
};

struct SurfaceHit {
  const ITerrainSolid* solid = nullptr;
  double y = 0.0;
};

struct PushOut {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  bool moved = false;
};

class TerrainWorld {
 public:
  void set_terrain(const std::vector<ITerrainInfo>& terrain);
  void clear();

  const std::vector<ITerrainSolid>& solids() const { return solids_; }

  const TerrainReachability* reach() const { return reach_.get(); }

  SurfaceHit surface_at(double x, double z, const SurfaceQuery& query = {}) const;

  void solids_at(double x, double y, double z,
                 std::vector<const ITerrainSolid*>& out) const;

  bool is_inside(double x, double y, double z) const;

  void push_out(PushOut& out, double from_x, double from_y, double from_z,
                double dir_x, double dir_z) const;

 private:
  std::vector<ITerrainSolid> solids_;
  std::unique_ptr<TerrainReachability> reach_;
};

}
}

