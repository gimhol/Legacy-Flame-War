#pragma once

#include <string>
#include <vector>

#include "lfw/defines/i_terrain_info.h"

namespace lfw {
namespace terrain {

enum class TerrainMode : int {
  Solid = 0,
  OneWay = 1,
  Through = 2,
  Cliff = 3,
};

struct ITerrainSolid {
  std::u16string id;
  double x1 = 0.0;
  double x2 = 0.0;
  double z1 = 0.0;
  double z2 = 0.0;
  double h1 = 0.0;
  double h2 = 0.0;
  int type = 0;
  TerrainMode mode = TerrainMode::Solid;
};

TerrainMode terrain_mode_of(const ITerrainInfo& seg);

double terrain_top(const ITerrainSolid& solid, double x, double z);

std::vector<ITerrainSolid> to_terrain_solids(const std::vector<ITerrainInfo>& terrain);

}
}
