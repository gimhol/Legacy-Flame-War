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

  godot::Ref<godot::ImageTexture> get_silhouette(const std::string& path);

  godot::Ref<godot::ImageTexture> get_tinted(const std::string& path, double r, double g, double b,
                                             double gray, double strength);

  long long loaded() const { return _loaded; }
  long long failed() const { return _failed; }
  long long silhouettes() const { return _silhouettes; }
  long long tinted() const { return _tinted; }
  const std::string& last_error() const { return _last_error; }

 private:
  /** 读 PNG。`path` 与会话里上一次相同就直接复用那张 `Image`，省一次解码。 */
  godot::Ref<godot::Image> read_image(const std::string& path, std::string& err);

  std::function<bool(const std::string&, std::string&)> _load;
  static constexpr std::size_t kMaxTinted = 64;
  /** 剪影是整张 sprite sheet 的副本（800×560 RGBA8 ≈ 1.7MB），不设上限会单调涨。 */
  static constexpr std::size_t kMaxSilhouettes = 64;
  struct Tinted {
    godot::Ref<godot::ImageTexture> tex;
    long long seq = 0;
  };
  struct Silhouette {
    godot::Ref<godot::ImageTexture> tex;
    long long seq = 0;
  };

  std::map<std::string, godot::Ref<godot::ImageTexture>> _cache;
  std::map<std::string, Silhouette> _sil;
  std::map<std::string, Tinted> _tint;
  long long _sil_seq = 0;
  long long _tint_seq = 0;
  /** 上一次解码出来的 `Image`（1 条就够：取色 → 剪影/染色 是连着来的）。 */
  std::string _last_image_path;
  godot::Ref<godot::Image> _last_image;
  long long _loaded = 0;
  long long _failed = 0;
  long long _silhouettes = 0;
  long long _tinted = 0;
  std::string _last_error;
};

}
