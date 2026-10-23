#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "lfw/lfw.h"
#include "lfw/ui/component/text_input.h"
#include "lfw/ui/uinode.h"
#include "lfw/world.h"

namespace lfw::host {

class HeadlessHost;

struct GodotModelSpec {
  std::string path;
  bool exact = true;
  std::string anim;
  bool loop = false, reverse = false, hold_last = false;
  double px = 0, py = 0, pz = 0;
  double sx = 1, sy = 1, sz = 1;
  double rx = 0, ry = 0, rz = 0;
  double outline_r = 0, outline_g = 0, outline_b = 0, outline_a = 0, outline_w = 0;
  /**
   * 画布像素 / 3D 世界单位（= 镜头 `scale_x`）。
   *
   * 3D 的 SubViewport 用正交相机、正交大小等于 viewport 边长，所以
   * 「1 世界单位 = 1 viewport 像素」；viewport 贴到画布时又乘了 `scale_x`。
   * `outline_w` 与 2D 同口径（画布像素），要换算成世界单位才能同宽。
   */
  double view_scale = 1;
};

struct GodotDrawItem {
  double x = 0, y = 0, w = 0, h = 0;
  std::string path;
  double tex_scale = 1.0;
  double sx = 0, sy = 0, sw = 0, sh = 0;
  bool flip_x = false;
  bool flip_y = false;
  double rot = 0;
  bool flat = false;
  GodotModelSpec model;
  bool has_model = false;
  double r = 1, g = 1, b = 1, a = 1;
  double mix_r = 1, mix_g = 1, mix_b = 1, mix_a = 0;
  double gray = 0;
  double outline_r = 0, outline_g = 0, outline_b = 0, outline_a = 0, outline_w = 0;
  std::string kind;
  std::string tag;
  double depth = 0;
  bool uv_tile = false;
  double uv_px = 0, uv_py = 0, uv_ox = 0, uv_oy = 0;
  int pass = 0;
  std::string text;
  double font_size = 0;
  int text_align = 0;
  double stroke_r = 0, stroke_g = 0, stroke_b = 0, stroke_a = 0, stroke_w = 0;
  bool world_text = false;
  bool text_below = false;
  bool has_clamp_x = false;
  double clamp_x0 = 0, clamp_x1 = 0, clamp_pad = 0;
  std::string font_css;
  bool has_clip = false;
  double clip_x0 = 0, clip_y0 = 0, clip_x1 = 0, clip_y1 = 0;
};

  inline constexpr double kModelViewport = 512.0;

struct GodotFrame {
  bool valid = false;
  long long index = 0;
  double dt = 0, lifetime = 0;
  double dfactor = 1, dtime = 0, tu = 0, fps = 0, ups = 0;
  bool dirty = false;
  double cam_p0_x = 0, cam_p0_y = 0, cam_p1_x = 0, cam_p1_y = 0;
  double cam_x = 0, cam_y = 0;
  double tx = 0, ty = 0, scale_x = 1, scale_y = 1;
  double screen_w = 794, screen_h = 450;
  std::string bg_id;
  std::vector<GodotDrawItem> items;
  int retained = 0, world_live = 0;
  int drawn_entities = 0, hidden_entities = 0, no_pic_entities = 0, no_file_entities = 0;
  int bg_layers = 0;
  int unnamed_textures = 0;
  int outline_items = 0;
  std::string outline_probe;
  int uv_items = 0;
  std::string uv_probe;
  int rotated_items = 0;
  double rotated_probe = 0;
  int model_items = 0;
  std::string model_probe;
};

struct GodotUIStats {
  long long layers = 0;
  long long nodes = 0;
  long long drawn = 0;
  long long hidden = 0;
  long long empty = 0;
  long long clipped = 0;
  long long missing_img = 0;
  long long texts = 0;
  long long items = 0;
};

struct GodotTextInputBox {
  ui::UINode* node = nullptr;
  ui::TextInput* input = nullptr;
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  double font_size = 9;
  std::string font_css;
  double r = 1, g = 1, b = 1, a = 1;
  int text_align = 0;
};

class GodotWorldRenderer : public IWorldRenderer {
 public:
  GodotWorldRenderer(LFW& lfw, HeadlessHost& host) : _lfw(&lfw), _host(&host) {}

  void add_entity(Entity& e) override;
  void del_entity(Entity& e) override;
  void render(double dt) override;
  void dispose() override;

  const GodotFrame& take_frame();
  bool has_frame() const { return _shown.valid; }

  bool read_resource(const std::string& path, std::string& out) const;

  struct Resolved {
    std::string path;
    double scale = 1;
    double img_w = 0, img_h = 0;
  };

  const Resolved& probe_path(const std::string& path) { return resolve_path(path); }

  void ui_begin_layer();
  void ui_append(GodotDrawItem item);
  void ui_end_layer();

  GodotUIStats& ui_stats() { return _ui_stats; }
  const GodotUIStats& ui_stats() const { return _ui_stats; }

  void ui_add_text_input(GodotTextInputBox box) { _ui_text_inputs.push_back(std::move(box)); }
  const std::vector<GodotTextInputBox>& ui_text_inputs() const { return _ui_text_inputs; }

  void set_ui_dump(bool on) { _ui_dump_on = on; }
  bool ui_dump_on() const { return _ui_dump_on; }
  std::vector<std::string>& ui_dump_lines() { return _ui_dump_lines; }

  long long added() const { return _added; }
  long long removed() const { return _removed; }
  long long renders() const { return _render_count; }
  long long live() const { return static_cast<long long>(_live.size()); }
  long long duplicate_adds() const { return _duplicate_adds; }
  long long stray_removes() const { return _stray_removes; }

 private:
  struct Live {
    Entity* e = nullptr;
    double p0x = 0, p0y = 0, p0z = 0;
    double p1x = 0, p1y = 0, p1z = 0;
    double rx = 0, ry = 0, rz = 0;
    bool has_r = false;
    double s0x = 0, s0y = 0, s0z = 0, s1x = 0, s1y = 0, s1z = 0;
    double s0s = 1, s1s = 1, s0o = 1, s1o = 1;
    std::u16string res_tex;
    double res_variant = 0;
    const void* res_data = nullptr;
    const Resolved* res = nullptr;
  };

  Live* find_live(const Entity* e);
  const Resolved& resolve_cached(Live& live, const std::u16string& tex, double variant);
  const Resolved& resolve_key(const Entity& e, const std::string& key, double variant);
  const Resolved& resolve_path(const std::string& path);
  void build_background(const World& world, GodotFrame& frame, bool foreground);
  void emit_sprite(GodotFrame& frame, const Entity& e, const Value& pic, double world_x,
                   double world_y, double anchor_x, double anchor_y, const Resolved& res,
                   const std::string& kind);
  void emit_entity_shadow(const World& world, GodotFrame& frame, const Entity& e, const Live& live);
  void emit_model(GodotFrame& frame, const Entity& e, const Value& model, double world_x,
                  double world_y, double shaking_x);
  void emit_world_text(GodotFrame& frame, const Entity& e, double world_x, double anchor_world_y,
                       bool below, const std::string& text, bool clamp_x);

  LFW* _lfw = nullptr;
  HeadlessHost* _host = nullptr;
  std::vector<Live> _live;
  std::vector<Live*> _order;
  std::unordered_map<const Entity*, Live*> _index;
  GodotFrame _pending;
  GodotFrame _shown;
  std::map<std::string, Resolved> _resolved;
  std::map<std::string, std::vector<std::string>> _variant_keys;
  std::vector<GodotDrawItem> _ui_buf;
  GodotUIStats _ui_stats;
  std::vector<GodotTextInputBox> _ui_text_inputs;
  bool _ui_dump_on = false;
  std::vector<std::string> _ui_dump_lines;
  int _render_count = 0;
  double _utime = 0, _dtime = 0, _dfactor = 1;
  double _uv_time = 0;
  bool _dirty = false;
  double _cam_p0_x = 0, _cam_p0_y = 0, _cam_p1_x = 0, _cam_p1_y = 0;
  long long _added = 0, _removed = 0, _duplicate_adds = 0, _stray_removes = 0;
  bool _disposed = false;
};

}
