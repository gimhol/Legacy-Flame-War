#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/data/ui_info.h"

namespace lfw::ui {

class UINode;

class UIActor {
 public:
  static constexpr const char* TAG = "Actor";

  using Handler = std::function<void(UINode&, const std::vector<Value>&)>;

  UIActor();

  UIActor& add(const std::u16string& key, Handler handler);

  void act(UINode& node, const std::vector<UIAction>& actions);

 private:
  std::map<std::u16string, Handler> _map;
};

UIActor& actor();

}
