#pragma once

#include <cstdint>
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
  godot::Ref<godot::ImageTexture> get_tinted(const std::string& path, double r, double g, double b,
                                             double gray, double strength);
  /**
   * 描边用的白剪影：同一张图、rgb 全白、alpha 原样。
   * 描边靠 `draw_texture_rect_region` 的 modulate 上色，而 modulate 是**乘法**，
   * 直接用精灵图会把队伍色乘上精灵自身的颜色 ⇒ 必须换成白色剪影再乘。
   */
  godot::Ref<godot::ImageTexture> get_silhouette(const std::string& path);

  long long loaded() const { return _loaded; }
  long long failed() const { return _failed; }
  long long tinted() const { return _tinted; }
  long long silhouettes() const { return _silhouettes; }
  const std::string& last_error() const { return _last_error; }

 private:
  /** 读 PNG。`path` 与会话里上一次相同就直接复用那张 `Image`，省一次解码。 */
  godot::Ref<godot::Image> read_image(const std::string& path, std::string& err);

  std::function<bool(const std::string&, std::string&)> _load;
  static constexpr std::size_t kMaxTinted = 64;
  static constexpr std::size_t kMaxSilhouettes = 128;
  struct Entry {
    godot::Ref<godot::ImageTexture> tex;
    long long seq = 0;
  };

  std::map<std::string, godot::Ref<godot::ImageTexture>> _cache;
  std::map<std::string, Entry> _tint;
  std::map<std::string, Entry> _silhouette;
  long long _tint_seq = 0;
  long long _silhouette_seq = 0;
  /** 上一次解码出来的 `Image`（1 条就够：取色 → 染色是连着来的）。 */
  std::string _last_image_path;
  godot::Ref<godot::Image> _last_image;
  long long _loaded = 0;
  long long _failed = 0;
  long long _tinted = 0;
  long long _silhouettes = 0;
  std::string _last_error;
};

}
