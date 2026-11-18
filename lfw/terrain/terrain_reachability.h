#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/defines/i_terrain_info.h"
#include "lfw/terrain/terrain_solid.h"

namespace lfw {
namespace terrain {

enum class TerrainReachDir : int {
  Left,
  Right,
  Far,
  Near,
  Up,
  Down,
};

enum class TerrainReachKind : int {
  Move,
  Jump,
  Drop,
};

struct TerrainReachPoint {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct TerrainReachEdge {
  const ITerrainInfo* from = nullptr;
  const ITerrainInfo* to = nullptr;
  TerrainReachDir dir = TerrainReachDir::Right;
  TerrainReachPoint from_point;
  TerrainReachPoint to_point;
  double gap = 0.0;
  double dy = 0.0;
  bool move = false;
  bool jump = false;
  bool drop = false;

  bool has(TerrainReachKind kind) const;
};

struct TerrainReachOptions {
  double step = 10.0;
  double jump_rise = 160.0;
  double jump_forward = 140.0;
  double max_drop = 480.0;
};

class TerrainReachability {
 public:
  explicit TerrainReachability(const std::vector<ITerrainInfo>& terrain,
                               const TerrainReachOptions& options = TerrainReachOptions());
  ~TerrainReachability();

  TerrainReachability(const TerrainReachability&) = delete;
  TerrainReachability& operator=(const TerrainReachability&) = delete;

  const TerrainReachOptions& options() const { return options_; }

  const std::vector<ITerrainInfo>& infos() const { return *source_; }

  const std::vector<TerrainReachEdge>& edges() const { return edges_; }

  const ITerrainInfo* find(const std::u16string& id) const;

  const std::vector<const TerrainReachEdge*>& edges_of(const ITerrainInfo& seg) const;

  std::vector<const ITerrainInfo*> neighbors(
      const ITerrainInfo& seg, TerrainReachKind kind = TerrainReachKind::Move) const;

  bool move_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                      std::optional<TerrainReachDir> dir = std::nullopt) const;

  bool jump_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                      std::optional<TerrainReachDir> dir = std::nullopt) const;

  bool drop_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                      std::optional<TerrainReachDir> dir = std::nullopt) const;

  bool reachable(const ITerrainInfo& from, const ITerrainInfo& to) const;

  std::optional<std::vector<const TerrainReachEdge*>> path(
      const ITerrainInfo& from, const ITerrainInfo& to,
      std::optional<TerrainReachKind> kind = std::nullopt) const;

 private:
  struct Seg {
    ITerrainSolid solid;
    const ITerrainInfo* source = nullptr;
  };

  std::optional<TerrainReachEdge> build_edge(const Seg& a, const Seg& b) const;

  bool has(const ITerrainInfo& from, const ITerrainInfo& to, TerrainReachKind kind,
           std::optional<TerrainReachDir> dir) const;

  TerrainReachOptions options_;
  const std::vector<ITerrainInfo>* source_ = nullptr;
  std::vector<Seg> segs_;
  std::vector<TerrainReachEdge> edges_;
  std::map<const ITerrainInfo*, std::vector<const TerrainReachEdge*>> edges_of_;
};

}
}


