#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/bg_info.h"
#include "lfw/data/keyed_map.h"
#include "lfw/defines/i_terrain_info.h"

namespace lfw {

/**
 * 背景数据（cook 后的 `.bg.xml` → `bg_data`）。
 *
 * 所有权：由 `DatMgr::resolve_bg_data()` 解析并长期持有，按 `const BackgroundData*` 持有 / 传递。
 */
class BackgroundData {
 public:
  BackgroundData() = default;
  explicit BackgroundData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const BgInfo& base() const { return _base; }
  const KeyedMap& dataset() const { return _dataset; }
  const std::vector<BgLayerInfo>& layers() const { return _layers; }
  const std::vector<ITerrainInfo>& terrain() const { return _terrain; }

  /** 表达式引擎 / JS 桥专用。业务代码别用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const BackgroundData& other) const;

 private:
  Value _raw;
  std::u16string _id;
  BgInfo _base;
  KeyedMap _dataset;
  std::vector<BgLayerInfo> _layers;
  std::vector<ITerrainInfo> _terrain;
};

}
