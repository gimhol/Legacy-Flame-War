#pragma once

#include <functional>
#include <map>
#include <string>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

namespace lfw::host {

class GodotTextureCache {
 public:
  explicit GodotTextureCache(std::function<bool(const std::string&, std::string&)> load)
      : _load(std::move(load)) {}

  godot::Ref<godot::ImageTexture> get(const std::string& path);

  /**
   * 描边层：把 4 次位移剪影合成为一张图 —— 每个像素的 alpha 是「4 个方向上
   * 半径 `radius`（纹理像素）内是否有实心像素」的叠加结果，按 `alpha` 逐次混合
   * 展开成 `1-(1-alpha)^k`，rgb 恒为白（颜色交给 modulate）。
   *
   * 绘制时一次 `draw_texture_rect_region` 就得到与「剪影 × 4 次位移」逐像素相同的
   * 结果（同色叠加展开就是同一个式子），绘制命令 5→2。
   */
  godot::Ref<godot::ImageTexture> get_outline_layer(const std::string& path, int radius,
                                                    double alpha);

  godot::Ref<godot::ImageTexture> get_tinted(const std::string& path, double r, double g, double b,
                                             double gray, double strength);

  long long loaded() const { return _loaded; }
  long long failed() const { return _failed; }
  long long outline_layers() const { return _outline_layers; }
  long long tinted() const { return _tinted; }
  const std::string& last_error() const { return _last_error; }

 private:
  /** 读 PNG。`path` 与会话里上一次相同就直接复用那张 `Image`，省一次解码。 */
  godot::Ref<godot::Image> read_image(const std::string& path, std::string& err);

  std::function<bool(const std::string&, std::string&)> _load;
  static constexpr std::size_t kMaxTinted = 64;
  /** 描边层是整张 sprite sheet 的副本（800×560 RGBA8 ≈ 1.7MB），不设上限会单调涨。 */
  static constexpr std::size_t kMaxOutlineLayers = 64;
  struct Tinted {
    godot::Ref<godot::ImageTexture> tex;
    long long seq = 0;
  };
  struct OutlineLayer {
    godot::Ref<godot::ImageTexture> tex;
    long long seq = 0;
  };

  std::map<std::string, godot::Ref<godot::ImageTexture>> _cache;
  std::map<std::string, OutlineLayer> _outline;
  std::map<std::string, Tinted> _tint;
  long long _outline_seq = 0;
  long long _tint_seq = 0;
  /** 上一次解码出来的 `Image`（1 条就够：取色 → 描边层/染色 是连着来的）。 */
  std::string _last_image_path;
  godot::Ref<godot::Image> _last_image;
  long long _loaded = 0;
  long long _failed = 0;
  long long _outline_layers = 0;
  long long _tinted = 0;
  std::string _last_error;
};

}
