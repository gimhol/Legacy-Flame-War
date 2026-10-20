#pragma once

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include "host/headless_host.h"
#include "host/thin/recording_world_renderer.h"

namespace lfw::host {

class ThinHost : public HeadlessHost {
 public:
  ThinHost(std::vector<std::filesystem::path> roots, bool dev)
      : HeadlessHost(std::move(roots), dev) {}

  IWorldRenderer* create_world_renderer(LFW& lfw) override {
    _recorder = std::make_unique<RecordingWorldRenderer>(lfw);
    return _recorder.get();
  }
  RecordingWorldRenderer& recorder() const { return *_recorder; }

  bool step_frame(lfw::World& world, double dt, int sub_render = 1);

 private:
  std::unique_ptr<RecordingWorldRenderer> _recorder;
};

}
