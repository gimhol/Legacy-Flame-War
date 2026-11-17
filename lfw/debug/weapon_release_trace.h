#pragma once

#include <string>
#include <vector>

namespace lfw {
namespace debug {

bool weapon_release_trace_enabled();
void set_weapon_release_trace_enabled(bool value);
void trace_weapon_release(const std::u16string& line);
std::vector<std::u16string> dump_weapon_release_traces();
void clear_weapon_release_traces();

}
}
