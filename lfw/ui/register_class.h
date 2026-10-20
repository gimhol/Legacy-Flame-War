#pragma once

#include <string>
#include <vector>

#include "lfw/utils/is_class.h"

namespace lfw {
namespace ui {

struct UIClassEntry {
  const ClazzTag* tag = nullptr;
  std::u16string name;
};

inline std::vector<UIClassEntry>& ui_class_registry() {
  static std::vector<UIClassEntry> list;
  return list;
}

inline void regist_ui_class(const ClazzTag* tag, const char16_t* name) {
  for (UIClassEntry& e : ui_class_registry()) {
    if (e.name == name) {
      e.tag = tag;
      return;
    }
  }
  ui_class_registry().push_back(UIClassEntry{tag, std::u16string(name)});
}

inline const ClazzTag* find_ui_class(const std::u16string& name) {
  for (const UIClassEntry& e : ui_class_registry()) {
    if (e.name == name) return e.tag;
  }
  return nullptr;
}

inline const std::u16string* ui_class_name(const ClazzTag* tag) {
  for (const UIClassEntry& e : ui_class_registry()) {
    if (e.tag == tag) return &e.name;
  }
  return nullptr;
}

}
}
