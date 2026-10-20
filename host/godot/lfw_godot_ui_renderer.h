#pragma once

#include <string>

#include "host/godot/lfw_godot_renderer.h"
#include "lfw/ui/ui_node_renderer.h"

namespace lfw::ui {
class UINode;
}

namespace lfw::host {

class GodotUINodeRenderer : public ui::IUINodeRenderer {
 public:
  explicit GodotUINodeRenderer(GodotWorldRenderer& world) : _world(&world) {}
  ~GodotUINodeRenderer() override;

  void render(double dt, double dfactor) override;
  void add(IUINodeRenderer& child) override;
  void del(IUINodeRenderer& child) override;
  IUINodeRenderer* parent() override { return _parent; }
  void set_x(double v) override { _pos_x = v; }
  void set_y(double v) override { _pos_y = v; }
  bool visible() const override { return _visible; }
  void set_visible(bool v) override { _visible = v; }

  void on_start() override;
  void on_stop() override;
  void on_resume() override;
  void on_pause() override;
  void del_self() override;

 private:
  void render_subtree(double dt);
  void step_local(double dt);
  void sync_model();
  void compute_global();
  void collect_text_input();
  bool emit_node();
  bool emit_image(const GodotDrawItem& proto, double x, double y, double w, double h,
                  bool mirror_x);
  bool emit_nine_patch(const GodotDrawItem& proto, double x, double y, double w, double h,
                       bool mirror_x);
  void parse_nine_patch(const Value& img);
  std::string node_tag() const;
  GodotUINodeRenderer* parent_renderer() const;
  int pass_of() const;

  GodotWorldRenderer* _world = nullptr;
  GodotUINodeRenderer* _parent = nullptr;
  bool _attached = true;
  bool _visible = true;

  bool _pos_init = false;
  double _pos_x = 0, _pos_y = 0;
  double _scale_x = 1, _scale_y = 1;
  double _gx = 0, _gy = 0, _gz = 0;
  double _gsx = 1, _gsy = 1;
  double _box_x0 = 0, _box_y0 = 0, _box_x1 = 0, _box_y1 = 0;
  bool _box_valid = false;
  bool _clip_on = false;
  double _clip_x0 = 0, _clip_y0 = 0, _clip_x1 = 0, _clip_y1 = 0;

  double _sync_lifetime = -1;
  bool _alpha_init = false;
  double _alpha = 1;
  double _rect_x = 0, _rect_y = 0, _rect_w = 0, _rect_h = 0;
  std::string _img_path;
  bool _wants_img = false;
  bool _img_unresolved = false;
  double _tex_scale = 1;
  double _img_lw = 0, _img_lh = 0;
  double _crop_x = 0, _crop_y = 0, _crop_w = 0, _crop_h = 0;
  bool _flip_x = false, _flip_y = false;
  double _repeat_x = 1, _repeat_y = 1;
  bool _np_on = false;
  double _np_fw = 1, _np_fh = 1;
  double _np_fl = 0, _np_ft = 0, _np_fr = 0, _np_fb = 0;
  double _np_lw = 0, _np_th = 0, _np_rw = 0, _np_bh = 0;
  double _anim_x = 0, _anim_y = 0, _anim_r = 0;
  double _uv_x = 0, _uv_y = 0, _uv_angle = 0;
  bool _has_color = false;
  double _cr = 1, _cg = 1, _cb = 1, _ca = 0;
  bool _has_bg = false, _has_fg = false;
  double _bgr = 0, _bgg = 0, _bgb = 0, _bga = 0;
  double _fgr = 0, _fgg = 0, _fgb = 0, _fga = 0;
  std::string _text;
  double _font_size = 9;
  std::string _font_css;
  int _text_align = 0;
  double _tr = 1, _tg = 1, _tb = 1, _ta = 1;
  double _sr = 0, _sg = 0, _sb = 0, _sa = 0, _stroke_w = 0;
};

}
