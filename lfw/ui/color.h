#pragma once

#include <optional>
#include <string>

#include "lfw/core/value.h"
#include "lfw/ui/rgba.h"

namespace lfw::ui {

std::optional<Rgba> hex_to_rgba(const std::u16string& hex);

std::optional<Rgba> int_to_rgba(double num);

struct RgbaLookup {
  bool found = false;
  bool is_null = false;
  Rgba value;
};
RgbaLookup rgba_map_get(const Value& key);

RgbaLookup parse_rgba(const Value& raw);

}
