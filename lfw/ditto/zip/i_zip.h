#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/ditto/zip/i_zip_object.h"

namespace lfw {

class IZip {
 public:
  virtual ~IZip() = default;
  virtual const std::u16string& name() const = 0;
  virtual IZipObject* file(const std::u16string& path) = 0;
  virtual std::vector<IZipObject*> file_regex(const std::u16string& pattern) = 0;
  virtual std::optional<std::u16string> md5() const { return std::nullopt; }
};

inline bool zip_name_matches(const std::u16string& name, const std::u16string& pattern) {
  const auto starts = [&name](const char16_t* const s) {
    const std::u16string p(s);
    return name.size() >= p.size() && name.compare(0, p.size(), p) == 0;
  };
  const auto ends = [&name](const char16_t* const s) {
    const std::u16string p(s);
    return name.size() >= p.size() && name.compare(name.size() - p.size(), p.size(), p) == 0;
  };
  if (pattern == u"\\.(i18n|strings)\\.json5?$") {
    return ends(u".i18n.json") || ends(u".i18n.json5") || ends(u".strings.json") ||
           ends(u".strings.json5");
  }
  if (pattern == u"\\.index\\.(json5|xml)$") {
    return ends(u".index.json5") || ends(u".index.xml");
  }
  if (pattern == u"bgm\\/.*?\\.mp3$") {
    return starts(u"bgm/") && ends(u".mp3");
  }
  if (pattern == u"^ui\\/.*?\\.ui\\.(json5?|xml)$") {
    return starts(u"ui/") &&
           (ends(u".ui.json") || ends(u".ui.json5") || ends(u".ui.xml"));
  }
  return name == pattern;
}

}
