#include "host/godot/lfw_godot_textures.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace lfw::host {

namespace {

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

godot::PackedByteArray to_bytes(const std::string& raw) {
  godot::PackedByteArray out;
  if (raw.empty()) return out;
  out.resize(static_cast<int64_t>(raw.size()));
  std::memcpy(out.ptrw(), raw.data(), raw.size());
  return out;
}

}

godot::Ref<godot::Image> GodotTextureCache::read_image(const std::string& path, std::string& err) {
  if (path == _last_image_path && _last_image.is_valid()) return _last_image;
  std::string raw;
  if (!_load(path, raw)) {
    err = "read failed: " + path;
    return {};
  }
  godot::Ref<godot::Image> image;
  image.instantiate();
  const godot::Error code = image->load_png_from_buffer(to_bytes(raw));
  if (code != godot::OK) {
    err = "png decode failed (" + std::to_string(static_cast<int>(code)) + "): " + path;
    return {};
  }
  _last_image_path = path;
  _last_image = image;
  return image;
}

godot::Ref<godot::ImageTexture> GodotTextureCache::get(const std::string& path) {
  if (path.empty()) return {};
  const auto found = _cache.find(path);
  if (found != _cache.end()) return found->second;

  godot::Ref<godot::ImageTexture> tex;
  std::string err;
  const godot::Ref<godot::Image> image = read_image(path, err);
  if (image.is_null()) {
    ++_failed;
    _last_error = err;
  } else {
    tex = godot::ImageTexture::create_from_image(image);
    if (tex.is_null()) {
      ++_failed;
      _last_error = "texture create failed: " + path;
    } else {
      ++_loaded;
    }
  }
  _cache.emplace(path, tex);
  return tex;
}

godot::Ref<godot::ImageTexture> GodotTextureCache::get_outline_layer(const std::string& path,
                                                                   int radius, double alpha,
                                                                   int32_t sx, int32_t sy,
                                                                   int32_t sw, int32_t sh) {
  if (path.empty() || radius < 1 || !(alpha > 0.0)) return {};
  // 区域进缓存键：同一张 sprite sheet 的每一帧采样范围不同，烘出来的层也不同
  char key[1152];
  std::snprintf(key, sizeof(key), "%s|%d|%.3f|%d,%d,%d,%d", path.c_str(), radius, alpha, sx, sy,
                sw, sh);
  const std::string cache_key(key);
  const auto found = _outline.find(cache_key);
  if (found != _outline.end()) {
    found->second.seq = ++_outline_seq;
    return found->second.tex;
  }

  godot::Ref<godot::ImageTexture> tex;
  std::string err;
  const godot::Ref<godot::Image> image = read_image(path, err);
  if (image.is_null()) {
    _last_error = err;
  } else {
    image->convert(godot::Image::FORMAT_RGBA8);
    const int32_t w = image->get_width();
    const int32_t h = image->get_height();
    godot::PackedByteArray data = image->get_data();
    const int64_t pixels = static_cast<int64_t>(w) * static_cast<int64_t>(h);
    if (w > 0 && h > 0 && data.size() == pixels * 4) {
      const std::uint8_t* const src = data.ptr();
      // 只在本帧的 UV 里取样/落盘（裁剪到图像范围内）
      const int32_t rx = sx < 0 ? 0 : (sx > w ? w : sx);
      const int32_t ry = sy < 0 ? 0 : (sy > h ? h : sy);
      const int32_t rex = sx + sw < 0 ? 0 : (sx + sw > w ? w : sx + sw);
      const int32_t rey = sy + sh < 0 ? 0 : (sy + sh > h ? h : sy + sh);
      std::vector<std::uint8_t> mask(static_cast<std::size_t>(pixels), 0);
      for (int64_t i = 0; i < pixels; ++i) mask[static_cast<std::size_t>(i)] = src[i * 4 + 3] >= 128 ? 1 : 0;
      // k 份同色 alpha 叠在一起 = 1-(1-alpha)^k
      std::uint8_t lut[5];
      for (int k = 0; k <= 4; ++k) {
        const double a = 1.0 - std::pow(1.0 - alpha, k);
        lut[k] = static_cast<std::uint8_t>(std::lround(clamp01(a) * 255.0));
      }
      std::vector<std::uint8_t> out(static_cast<std::size_t>(pixels) * 4, 0);
      for (int32_t y = ry; y < rey; ++y) {
        const int32_t yu = y - radius;
        const int32_t yd = y + radius;
        const int64_t row = static_cast<int64_t>(y) * w;
        for (int32_t x = rx; x < rex; ++x) {
          // 4 次位移 blit：像素 (x,y) 收到的是被移动过来的 (x,y±r) / (x±r,y)
          // 取样同样不出本帧 UV：图集里相邻帧的像素不能算作角色本体
          int k = 0;
          if (yu >= ry && mask[static_cast<std::size_t>(static_cast<int64_t>(yu) * w + x)]) ++k;
          if (yd < rey && mask[static_cast<std::size_t>(static_cast<int64_t>(yd) * w + x)]) ++k;
          if (x - radius >= rx && mask[static_cast<std::size_t>(row + x - radius)]) ++k;
          if (x + radius < rex && mask[static_cast<std::size_t>(row + x + radius)]) ++k;
          if (k == 0) continue;
          std::uint8_t* const px = out.data() + (row + x) * 4;
          px[0] = 255;
          px[1] = 255;
          px[2] = 255;
          px[3] = lut[k];
        }
      }
      godot::PackedByteArray baked;
      baked.resize(static_cast<int64_t>(out.size()));
      std::memcpy(baked.ptrw(), out.data(), out.size());
      const godot::Ref<godot::Image> layer_img =
          godot::Image::create_from_data(w, h, false, godot::Image::FORMAT_RGBA8, baked);
      if (!layer_img.is_null()) tex = godot::ImageTexture::create_from_image(layer_img);
    }
    if (tex.is_null()) _last_error = "outline layer failed: " + path;
    else ++_outline_layers;
  }
  if (_outline.size() >= kMaxOutlineLayers) {
    auto oldest = _outline.begin();
    for (auto it = _outline.begin(); it != _outline.end(); ++it) {
      if (it->second.seq < oldest->second.seq) oldest = it;
    }
    _outline.erase(oldest);
  }
  _outline.emplace(cache_key, OutlineLayer{tex, ++_outline_seq});
  return tex;
}

godot::Ref<godot::ImageTexture> GodotTextureCache::get_tinted(const std::string& path, double r,
                                                             double g, double b, double gray,
                                                             double strength) {
  if (path.empty()) return {};
  char key[768];
  std::snprintf(key, sizeof(key), "%s|%.3f,%.3f,%.3f|%.3f|%.3f", path.c_str(), r, g, b, gray,
                strength);
  const std::string cache_key(key);
  const auto found = _tint.find(cache_key);
  if (found != _tint.end()) {
    found->second.seq = ++_tint_seq;
    return found->second.tex;
  }

  godot::Ref<godot::ImageTexture> tex;
  std::string err;
  const godot::Ref<godot::Image> image = read_image(path, err);
  if (image.is_null()) {
    _last_error = err;
  } else {
    image->convert(godot::Image::FORMAT_RGBA8);
    const int32_t w = image->get_width();
    const int32_t h = image->get_height();
    godot::PackedByteArray data = image->get_data();
    const int64_t pixels = static_cast<int64_t>(w) * static_cast<int64_t>(h);
    if (w > 0 && h > 0 && data.size() == pixels * 4) {
      const double gr = gray < 0.0 ? 0.0 : (gray > 1.0 ? 1.0 : gray);
      const double ms = strength < 0.0 ? 0.0 : (strength > 1.0 ? 1.0 : strength);
      const auto to_byte = [](double v) {
        return static_cast<std::uint8_t>(std::lround(clamp01(v) * 255.0));
      };
      const double tr = clamp01(r);
      const double tg = clamp01(g);
      const double tb = clamp01(b);
      std::uint8_t* const bytes = data.ptrw();
      for (int64_t i = 0; i < pixels; ++i) {
        std::uint8_t* const px = bytes + i * 4;
        if (px[3] == 0) continue;
        double cr = static_cast<double>(px[0]) / 255.0;
        double cg = static_cast<double>(px[1]) / 255.0;
        double cb = static_cast<double>(px[2]) / 255.0;
        if (gr > 0.0) {
          const double lum = 0.299 * cr + 0.587 * cg + 0.114 * cb;
          cr += (lum - cr) * gr;
          cg += (lum - cg) * gr;
          cb += (lum - cb) * gr;
        }
        if (ms > 0.0) {
          cr += (tr - cr) * ms;
          cg += (tg - cg) * ms;
          cb += (tb - cb) * ms;
        }
        px[0] = to_byte(cr);
        px[1] = to_byte(cg);
        px[2] = to_byte(cb);
      }
      const godot::Ref<godot::Image> baked =
          godot::Image::create_from_data(w, h, false, godot::Image::FORMAT_RGBA8, data);
      if (!baked.is_null()) tex = godot::ImageTexture::create_from_image(baked);
    }
    if (tex.is_null()) _last_error = "tint failed: " + path;
    else ++_tinted;
  }
  if (_tint.size() >= kMaxTinted) {
    auto oldest = _tint.begin();
    for (auto it = _tint.begin(); it != _tint.end(); ++it) {
      if (it->second.seq < oldest->second.seq) oldest = it;
    }
    _tint.erase(oldest);
  }
  _tint.emplace(cache_key, Tinted{tex, ++_tint_seq});
  return tex;
}

}
