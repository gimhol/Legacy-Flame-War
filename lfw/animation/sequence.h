#pragma once

#include <optional>
#include <vector>

#include "lfw/animation/animation.h"

namespace lfw {

class Sequence : public Animation {
 public:
  explicit Sequence(std::vector<Animation*> anims = {});

  Animation& start(std::optional<bool> reverse = std::nullopt) override;
  Animation& end(std::optional<bool> reverse = std::nullopt) override;
  Animation& calc() override;

  Animation* curr_anim() const { return _curr_anim; }
  const std::vector<Animation*>& anims() const { return _anims; }
  std::vector<Animation*>& anims() { return _anims; }

 private:
  std::vector<Animation*> _anims;
  Animation* _curr_anim = nullptr;
};

}
