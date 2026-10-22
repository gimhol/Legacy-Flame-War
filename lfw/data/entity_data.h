#pragma once

#include <string>

#include "lfw/core/value.h"
#include "lfw/data/frame_info.h"
#include "lfw/data/hit_key_map.h"
#include "lfw/data/next_frame.h"
#include "lfw/data/picture_info.h"

namespace lfw {

/**
 * 实体数据（cook 后的 `data/<id>.obj.json5`）。
 *
 * 这是一个具体类型：顶层字段在构造时取成强类型成员，字段表
 * （`base` / `indexes` …）以 `Object` 视图暴露，`frames` 是解析好的 `FrameInfo` map。
 * 只有表达式引擎、快照这类必须拿到 JS 值的场合才用 `to_value()`。
 */
class EntityData {
 public:
  EntityData() = default;
  explicit EntityData(Value raw);

  bool valid() const { return as_object(_raw) != nullptr; }

  const std::u16string& id() const { return _id; }
  const std::u16string& alias_id() const { return _alias_id; }
  double type() const { return _type; }
  bool processed() const { return _processed; }

  /** `base.type`：没有就 0。 */
  double base_type() const;
  const Object& base() const;
  /** `frames`：帧 id → 帧信息（已在加载期解析好）。 */
  const FrameInfoMap& frames() const { return _frames; }
  /** 按帧 id 查；找不到返回 `nullptr`。 */
  const FrameInfo* find_frame(const std::u16string& id) const {
    return find_frame_info(_frames, id);
  }
  const Object& indexes() const;
  /** 顶层字段是否存在（哪怕值是 null/空对象）。 */
  bool has_field(const char16_t* key) const;

  const NextFrame& on_dead() const { return _on_dead; }
  const NextFrame& on_exhaustion() const { return _on_exhaustion; }
  const HitKeyMap& pre_hitkeys() const { return _pre_hitkeys; }
  const HitKeyMap& post_hitkeys() const { return _post_hitkeys; }
  const HitKeyMap& pre_hitkeys_map() const { return _pre_hitkeys_map; }
  const HitKeyMap& post_hitkeys_map() const { return _post_hitkeys_map; }
  const Value& bdy_prefabs() const { return _bdy_prefabs; }
  const Value& itr_prefabs() const { return _itr_prefabs; }
  /** `base.files`：贴图 id → 图片信息。 */
  const PictureInfoMap& files() const { return _files; }
  /** `base.models`：模型 id → 模型信息。 */
  const ModelInfoMap& models() const { return _models; }

  /** 表达式引擎 / JS 桥专用。业务代码别用。 */
  const Value& to_value() const { return _raw; }

  bool same_as(const EntityData& other) const;

 private:
  Value _raw;
  std::u16string _id;
  std::u16string _alias_id;
  double _type = 0;
  bool _processed = false;
  NextFrame _on_dead;
  NextFrame _on_exhaustion;
  HitKeyMap _pre_hitkeys;
  HitKeyMap _post_hitkeys;
  HitKeyMap _pre_hitkeys_map;
  HitKeyMap _post_hitkeys_map;
  Value _bdy_prefabs;
  Value _itr_prefabs;
  FrameInfoMap _frames;
  PictureInfoMap _files;
  ModelInfoMap _models;
};

}
