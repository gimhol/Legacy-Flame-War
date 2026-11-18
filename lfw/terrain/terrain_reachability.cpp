#include "lfw/terrain/terrain_reachability.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace lfw {
namespace terrain {

namespace {

constexpr double kEps = 0.5;

double min2(double a, double b) { return a < b ? a : b; }

double max2(double a, double b) { return a > b ? a : b; }

}

bool TerrainReachEdge::has(TerrainReachKind kind) const {
  switch (kind) {
    case TerrainReachKind::Move:
      return move;
    case TerrainReachKind::Jump:
      return jump;
    case TerrainReachKind::Drop:
      return drop;
  }
  return false;
}

TerrainReachability::TerrainReachability(const std::vector<ITerrainInfo>& terrain,
                                         const TerrainReachOptions& options)
    : options_(options), source_(&terrain) {
  segs_.reserve(terrain.size());
  for (const ITerrainInfo& info : terrain) {
    const TerrainMode mode = terrain_mode_of(info);
    if (mode == TerrainMode::Cliff) continue;
    Seg seg;
    seg.source = &info;
    seg.solid.id = info.id;
    seg.solid.x1 = info.x1;
    seg.solid.x2 = info.x2;
    seg.solid.z1 = info.z1;
    seg.solid.z2 = info.z2;
    seg.solid.h1 = info.h1;
    seg.solid.h2 = info.h2;
    seg.solid.type = info.type;
    seg.solid.mode = mode;
    segs_.push_back(seg);
  }

  for (size_t i = 0; i < segs_.size(); ++i) {
    for (size_t j = 0; j < segs_.size(); ++j) {
      if (i == j) continue;
      const std::optional<TerrainReachEdge> edge = build_edge(segs_[i], segs_[j]);
      if (!edge.has_value()) continue;
      edges_.push_back(*edge);
    }
  }
  for (const TerrainReachEdge& edge : edges_) {
    edges_of_[edge.from].push_back(&edge);
  }
}

TerrainReachability::~TerrainReachability() = default;

const ITerrainInfo* TerrainReachability::find(const std::u16string& id) const {
  for (const ITerrainInfo& info : *source_) {
    if (info.id == id) return &info;
  }
  return nullptr;
}

const std::vector<const TerrainReachEdge*>& TerrainReachability::edges_of(
    const ITerrainInfo& seg) const {
  static const std::vector<const TerrainReachEdge*> kEmpty;
  const auto it = edges_of_.find(&seg);
  return it == edges_of_.end() ? kEmpty : it->second;
}

std::vector<const ITerrainInfo*> TerrainReachability::neighbors(
    const ITerrainInfo& seg, TerrainReachKind kind) const {
  std::vector<const ITerrainInfo*> ret;
  for (const TerrainReachEdge* const edge : edges_of(seg)) {
    if (!edge->has(kind)) continue;
    if (std::find(ret.begin(), ret.end(), edge->to) != ret.end()) continue;
    ret.push_back(edge->to);
  }
  return ret;
}

bool TerrainReachability::move_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                                         std::optional<TerrainReachDir> dir) const {
  return has(from, to, TerrainReachKind::Move, dir);
}

bool TerrainReachability::jump_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                                         std::optional<TerrainReachDir> dir) const {
  return has(from, to, TerrainReachKind::Jump, dir);
}

bool TerrainReachability::drop_reachable(const ITerrainInfo& from, const ITerrainInfo& to,
                                         std::optional<TerrainReachDir> dir) const {
  return has(from, to, TerrainReachKind::Drop, dir);
}

bool TerrainReachability::reachable(const ITerrainInfo& from, const ITerrainInfo& to) const {
  return has(from, to, TerrainReachKind::Move, std::nullopt) ||
         has(from, to, TerrainReachKind::Jump, std::nullopt) ||
         has(from, to, TerrainReachKind::Drop, std::nullopt);
}

bool TerrainReachability::has(const ITerrainInfo& from, const ITerrainInfo& to,
                              TerrainReachKind kind,
                              std::optional<TerrainReachDir> dir) const {
  for (const TerrainReachEdge* const edge : edges_of(from)) {
    if (edge->to != &to) continue;
    if (!edge->has(kind)) continue;
    if (dir.has_value() && edge->dir != *dir) continue;
    return true;
  }
  return false;
}

std::optional<std::vector<const TerrainReachEdge*>> TerrainReachability::path(
    const ITerrainInfo& from, const ITerrainInfo& to,
    std::optional<TerrainReachKind> kind) const {
  if (&from == &to) return std::vector<const TerrainReachEdge*>();
  std::set<const ITerrainInfo*> seen;
  seen.insert(&from);
  std::map<const ITerrainInfo*, const TerrainReachEdge*> prev;
  std::vector<const ITerrainInfo*> queue;
  queue.push_back(&from);
  for (size_t i = 0; i < queue.size(); ++i) {
    for (const TerrainReachEdge* const edge : edges_of(*queue[i])) {
      if (kind.has_value() && !edge->has(*kind)) continue;
      if (seen.count(edge->to) != 0) continue;
      seen.insert(edge->to);
      prev[edge->to] = edge;
      if (edge->to == &to) {
        std::vector<const TerrainReachEdge*> ret;
        const TerrainReachEdge* step = edge;
        while (step != nullptr) {
          ret.push_back(step);
          if (step->from == &from) break;
          const auto it = prev.find(step->from);
          step = it == prev.end() ? nullptr : it->second;
        }
        std::reverse(ret.begin(), ret.end());
        return ret;
      }
      queue.push_back(edge->to);
    }
  }
  return std::nullopt;
}

std::optional<TerrainReachEdge> TerrainReachability::build_edge(const Seg& a,
                                                                const Seg& b) const {
  const ITerrainSolid& sa = a.solid;
  const ITerrainSolid& sb = b.solid;
  const double xo = min2(sa.x2, sb.x2) - max2(sa.x1, sb.x1);
  const double zo = min2(sa.z2, sb.z2) - max2(sa.z1, sb.z1);

  TerrainReachDir dir = TerrainReachDir::Right;
  double gap = 0.0;
  double fx = 0.0;
  double fy = 0.0;
  double fz = 0.0;
  double tx = 0.0;
  double ty = 0.0;
  double tz = 0.0;

  if (zo >= kEps && sb.x1 >= sa.x2 - kEps) {
    dir = TerrainReachDir::Right;
    gap = max2(0.0, sb.x1 - sa.x2);
    const double z = (max2(sa.z1, sb.z1) + min2(sa.z2, sb.z2)) / 2.0;
    fx = sa.x2;
    fz = z;
    tx = sb.x1;
    tz = z;
    fy = terrain_top(sa, fx, fz);
    ty = terrain_top(sb, tx, tz);
  } else if (zo >= kEps && sa.x1 >= sb.x2 - kEps) {
    dir = TerrainReachDir::Left;
    gap = max2(0.0, sa.x1 - sb.x2);
    const double z = (max2(sa.z1, sb.z1) + min2(sa.z2, sb.z2)) / 2.0;
    fx = sa.x1;
    fz = z;
    tx = sb.x2;
    tz = z;
    fy = terrain_top(sa, fx, fz);
    ty = terrain_top(sb, tx, tz);
  } else if (xo >= kEps && sb.z1 >= sa.z2 - kEps) {
    dir = TerrainReachDir::Near;
    gap = max2(0.0, sb.z1 - sa.z2);
    const double x = (max2(sa.x1, sb.x1) + min2(sa.x2, sb.x2)) / 2.0;
    fx = x;
    fz = sa.z2;
    tx = x;
    tz = sb.z1;
    fy = terrain_top(sa, fx, fz);
    ty = terrain_top(sb, tx, tz);
  } else if (xo >= kEps && sa.z1 >= sb.z2 - kEps) {
    dir = TerrainReachDir::Far;
    gap = max2(0.0, sa.z1 - sb.z2);
    const double x = (max2(sa.x1, sb.x1) + min2(sa.x2, sb.x2)) / 2.0;
    fx = x;
    fz = sa.z1;
    tx = x;
    tz = sb.z2;
    fy = terrain_top(sa, fx, fz);
    ty = terrain_top(sb, tx, tz);
  } else if (xo >= kEps && zo >= kEps) {
    const double x = (max2(sa.x1, sb.x1) + min2(sa.x2, sb.x2)) / 2.0;
    const double z = (max2(sa.z1, sb.z1) + min2(sa.z2, sb.z2)) / 2.0;
    fx = x;
    fy = terrain_top(sa, x, z);
    fz = z;
    tx = x;
    ty = terrain_top(sb, x, z);
    tz = z;
    dir = ty - fy >= 0.0 ? TerrainReachDir::Up : TerrainReachDir::Down;
  } else {
    return std::nullopt;
  }

  const double dy = ty - fy;
  const double step = options_.step;
  const bool move = std::abs(dy) <= step && gap <= step;
  const bool jump = dy > step
                        ? dy <= options_.jump_rise && gap <= options_.jump_forward
                        : gap > step && gap <= options_.jump_forward &&
                              dy >= -options_.max_drop;
  const bool drop = dy < -step && dy >= -options_.max_drop && gap <= step;
  if (!move && !jump && !drop) return std::nullopt;

  TerrainReachEdge ret;
  ret.from = a.source;
  ret.to = b.source;
  ret.dir = dir;
  ret.from_point = TerrainReachPoint{fx, fy, fz};
  ret.to_point = TerrainReachPoint{tx, ty, tz};
  ret.gap = gap;
  ret.dy = dy;
  ret.move = move;
  ret.jump = jump;
  ret.drop = drop;
  return ret;
}

}
}

