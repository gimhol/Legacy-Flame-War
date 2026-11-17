#include "lfw/terrain/terrain_solid.h"

#include <cmath>
#include <string>
#include <vector>

#include "lfw/defines/i_terrain_info.h"
#include "lfw/utils/math/clamp.h"

namespace lfw {
namespace terrain {

TerrainMode terrain_mode_of(const ITerrainInfo& seg) {
  if (seg.cliff) return TerrainMode::Cliff;
  if (seg.through) return TerrainMode::Through;
  if (seg.one_way) return TerrainMode::OneWay;
  return TerrainMode::Solid;
}

double terrain_top(const ITerrainSolid& solid, double x, double z) {
  switch (solid.type) {
    case static_cast<int>(TerrainEnum::SlopeH): {
      if (solid.x1 == solid.x2) return solid.h1;
      const double t = (clamp(x, solid.x1, solid.x2) - solid.x1) / (solid.x2 - solid.x1);
      return solid.h1 + t * (solid.h2 - solid.h1);
    }
    case static_cast<int>(TerrainEnum::SlopeV): {
      if (solid.z1 == solid.z2) return solid.h1;
      const double t = (clamp(z, solid.z1, solid.z2) - solid.z1) / (solid.z2 - solid.z1);
      return solid.h1 + t * (solid.h2 - solid.h1);
    }
    default:
      return solid.h1;
  }
}

std::vector<ITerrainSolid> to_terrain_solids(const std::vector<ITerrainInfo>& terrain) {
  std::vector<ITerrainSolid> ret;
  ret.reserve(terrain.size());
  for (const ITerrainInfo& seg : terrain) {
    ITerrainSolid solid;
    solid.id = seg.id;
    solid.x1 = seg.x1;
    solid.x2 = seg.x2;
    solid.z1 = seg.z1;
    solid.z2 = seg.z2;
    solid.h1 = seg.h1;
    solid.h2 = seg.h2;
    solid.type = seg.type;
    solid.mode = terrain_mode_of(seg);
    ret.push_back(std::move(solid));
  }
  return ret;
}

}
}
