#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/world.h"

namespace lfw::host {

struct EntityDrawRecord {
  std::string id;
  std::string data_id;
  double type = 0;
  double x = 0, y = 0, z = 0;
  double px = 0, py = 0, pz = 0;
  double lx = 0, ly = 0, lz = 0;
  double facing = 1;
  double wait = 0;
  std::string frame_id;
  std::string state;
  std::string prev_frame_id;
  std::string pic_tex;
  double pic_x = 0, pic_y = 0, pic_w = 0, pic_h = 0;
  double pics_count = 0;
  double frame_cx = 0, frame_cy = 0;
  double hp = 0, hp_max = 0, mp = 0, mp_max = 0;
  double toughness = 0, toughness_max = 0;
  double invisible = 0, blinking = 0, invulnerable = 0;
  double name_visible = 0, ctrl_visible = 0;
  std::string name;
  double variant = 0;
  double shaking = 0;
  double lifetime = 0;
  std::string mix_color;
  double mix_strength = 0, greyscale = 0;
  std::string outline_color;
  double outline_alpha = 0, outline_width = 0;
  std::string outline_enabled;
  double render_effect_time = 0;
  double ghosted = 0, mounted = 0;
};

struct FrameDrawRecord {
  int index = 0;
  double dt = 0;
  double lifetime = 0;
  double dfactor = 1;
  double dtime = 0;
  bool dirty = false;
  double tu = 0;
  double fps = 0;
  double ups = 0;
  double cam_p0_x = 0, cam_p0_y = 0;
  double cam_p1_x = 0, cam_p1_y = 0;
  double cam_lerp_x = 0, cam_lerp_y = 0;
  int retained = 0;
  int world_live = 0;
  bool retained_matches_world = true;
  double cam_x = 0, cam_y = 0;
  std::string bg_id;
  double bg_layer_data_index = 0, bg_update_times = 0;
  double tx = 0, ty = 0, tz = 0;
  double tsx = 1, tsy = 1, tsz = 1;
  double screen_w = 0, screen_h = 0;
  std::vector<EntityDrawRecord> entities;
};

std::string format_frame_header(const FrameDrawRecord& frame);
std::string format_entity_record(const EntityDrawRecord& entity);
std::string format_entity_field_names();

class RecordingWorldRenderer : public IWorldRenderer {
 public:
  explicit RecordingWorldRenderer(LFW& lfw) : _lfw(&lfw) {}

  void add_entity(Entity& e) override;
  void del_entity(Entity& e) override;
  void render(double dt) override;
  void dispose() override;

  int render_count() const { return _render_count; }
  const std::vector<FrameDrawRecord>& frames() const { return _frames; }
  std::size_t retained_count() const { return _live.size(); }
  long long added() const { return _added; }
  long long removed() const { return _removed; }
  long long duplicate_adds() const { return _duplicate_adds; }
  long long stray_removes() const { return _stray_removes; }
  long long mismatched_frames() const { return _mismatched_frames; }
  bool disposed() const { return _disposed; }
  long long entity_records() const { return _entity_records; }
  long long df_between_records() const { return _df_between; }
  long long interpolated_records() const { return _interpolated; }
  long long moving_records() const { return _moving; }

 private:
  struct Live {
    Entity* e = nullptr;
    double p0x = 0, p0y = 0, p0z = 0;
    double p1x = 0, p1y = 0, p1z = 0;
  };

  LFW* _lfw = nullptr;
  std::vector<Live> _live;
  std::vector<FrameDrawRecord> _frames;
  int _render_count = 0;
  double _utime = 0;
  double _dtime = 0;
  double _dfactor = 1;
  bool _dirty = false;
  double _cam_p0_x = 0, _cam_p0_y = 0;
  double _cam_p1_x = 0, _cam_p1_y = 0;
  long long _added = 0;
  long long _removed = 0;
  long long _duplicate_adds = 0;
  long long _stray_removes = 0;
  long long _mismatched_frames = 0;
  long long _entity_records = 0;
  long long _df_between = 0;
  long long _interpolated = 0;
  long long _moving = 0;
  bool _disposed = false;
};

}
