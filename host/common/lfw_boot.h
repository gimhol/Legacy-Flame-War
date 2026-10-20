#pragma once

#include <functional>
#include <string>
#include <vector>

namespace lfw {
class LFW;
class World;
}

namespace lfw::host {

struct SpawnInfo {
  std::u16string player_id;
  std::u16string fighter_id;
  bool vs = false;
  double x = 0;
  double y = 0;
  double z = 0;
  std::u16string team;
};

struct BootFailure {
  std::u16string fighter_id;
  bool create_failed = false;
};

struct BootOptions {
  std::vector<std::u16string> fighters;
  std::u16string stage;
  int players = 2;
  bool vs = false;
  std::function<void(const SpawnInfo&)> on_spawn;
  std::function<bool(const BootFailure&)> on_failure;
};

struct BootResult {
  bool ok = false;
  std::u16string stage_id;
  std::u16string bg_id;
};

BootResult boot_scene(lfw::LFW& lfw, const BootOptions& options);

std::string frame_digest(const lfw::World& world, int frame);

double total_hp(const lfw::World& world);

}
