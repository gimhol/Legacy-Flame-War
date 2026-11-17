#include "lfw/debug/weapon_release_trace.h"

#include <string>
#include <vector>

namespace lfw {
namespace debug {

namespace {

constexpr size_t kMaxTraces = 32;
std::vector<std::u16string> g_traces;
bool g_enabled = false;

}

bool weapon_release_trace_enabled() { return g_enabled; }

void set_weapon_release_trace_enabled(bool value) { g_enabled = value; }

void trace_weapon_release(const std::u16string& line) {
  g_traces.push_back(line);
  if (g_traces.size() > kMaxTraces) g_traces.erase(g_traces.begin());
}

std::vector<std::u16string> dump_weapon_release_traces() { return g_traces; }

void clear_weapon_release_traces() { g_traces.clear(); }

}
}
