#pragma once

#include <godot_cpp/classes/font.hpp>
#include <map>
#include <string>
#include <vector>

namespace lfw::host {

struct CssFont {
  std::string family;
  int size_px = 0;
  int weight = 400;
  bool italic = false;
  std::string key() const;
};

CssFont parse_css_font(const std::string& css);

class FontCache {
 public:
  void set_fallback(godot::Ref<godot::Font> fallback);
  const godot::Ref<godot::Font>& fallback() const { return _fallback; }

  godot::Ref<godot::Font> get(const CssFont& spec);
  /** 等宽字体（`monospace` / `mono` 走这条）：按 Consolas → Cascadia Mono → … 找一个在的 */
  godot::Ref<godot::Font> mono(int weight = 400);

  std::size_t resolved_count() const { return _resolved.size(); }
  const std::vector<std::string>& missed() const { return _missed; }
  std::string describe() const;

 private:
  godot::Ref<godot::Font> _fallback;
  std::map<std::string, godot::Ref<godot::Font>> _resolved;
  std::vector<std::string> _missed;
};

}
