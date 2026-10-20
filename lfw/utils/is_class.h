#pragma once

namespace lfw {

struct ClazzTag {
  const ClazzTag* parent = nullptr;
};

inline bool is_class(const ClazzTag* cls, const ClazzTag* clazz) {
  for (const ClazzTag* c = cls; c != nullptr; c = c->parent) {
    if (c == clazz) return true;
  }
  return false;
}

}
