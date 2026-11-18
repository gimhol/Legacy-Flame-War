#include "lfw/terrain/terrain_world.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "lfw/defines/i_terrain_info.h"
#include "lfw/terrain/terrain_solid.h"

namespace lfw {
namespace terrain {

namespace {

constexpr double kEps = 0.5;

}

void TerrainWorld::clear() {
  solids_.clear();
  reach_.reset();
}

void TerrainWorld::set_terrain(const std::vector<ITerrainInfo>& terrain) {
  solids_ = to_terrain_solids(terrain);
  reach_ = std::make_unique<TerrainReachability>(terrain);
}

SurfaceHit TerrainWorld::surface_at(double x, double z, const SurfaceQuery& query) const {
  SurfaceHit ret;
  bool has = false;
  for (const ITerrainSolid& solid : solids_) {
    if (x < solid.x1 || x > solid.x2 || z < solid.z1 || z > solid.z2) continue;
    const double top = terrain_top(solid, x, z);
    const bool passable =
        solid.mode == TerrainMode::OneWay || solid.mode == TerrainMode::Through;
    if (passable && query.has_y) {
      if (query.rising) {
        if (query.y <= top) continue;
      } else if (solid.mode == TerrainMode::Through && query.drop_through) {
        if (query.y <= top + kEps && query.y >= top - query.step) continue;
      } else if (query.y < top - kEps) {
        if (!query.has_prev_y || query.prev_y < top) continue;
      }
    }
    if (!has || top > ret.y) {
      ret.solid = &solid;
      ret.y = top;
      has = true;
    }
  }
  return ret;
}

void TerrainWorld::solids_at(double x, double y, double z,
                             std::vector<const ITerrainSolid*>& out) const {
  out.clear();
  for (const ITerrainSolid& solid : solids_) {
    if (solid.mode != TerrainMode::Solid) continue;
    if (x < solid.x1 || x > solid.x2 || z < solid.z1 || z > solid.z2) continue;
    if (y >= terrain_top(solid, x, z) - kEps) continue;
    out.push_back(&solid);
  }
}

bool TerrainWorld::is_inside(double x, double y, double z) const {
  for (const ITerrainSolid& solid : solids_) {
    if (solid.mode != TerrainMode::Solid) continue;
    if (x < solid.x1 || x > solid.x2 || z < solid.z1 || z > solid.z2) continue;
    if (y >= terrain_top(solid, x, z) - kEps) continue;
    return true;
  }
  return false;
}

void TerrainWorld::push_out(PushOut& out, double from_x, double from_y,
                             double from_z, double dir_x, double dir_z) const {
  out.x = from_x;
  out.y = from_y;
  out.z = from_z;
  out.moved = false;
  if (!is_inside(from_x, from_y, from_z)) return;

  const double len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
  if (len > 1e-6) {
    const double ux = dir_x / len;
    const double uz = dir_z / len;
    double prev = 0.0;
    double next = 0.0;
    bool found = false;
    for (int i = 1; i <= 512; ++i) {
      const double d = static_cast<double>(i) * 0.5;
      if (is_inside(from_x + ux * d, from_y, from_z + uz * d)) {
        prev = d;
        continue;
      }
      next = d;
      found = true;
      break;
    }
    if (found) {
      for (int i = 0; i < 12; ++i) {
        const double mid = (prev + next) / 2.0;
        if (is_inside(from_x + ux * mid, from_y, from_z + uz * mid)) prev = mid;
        else next = mid;
      }
      const double d = next + 1.0;
      out.x = from_x + ux * d;
      out.z = from_z + uz * d;
      out.moved = true;
      return;
    }
  }

  out.y = surface_at(from_x, from_z, SurfaceQuery{}).y;
  out.moved = true;
}

}
}

