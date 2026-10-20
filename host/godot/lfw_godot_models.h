#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <godot_cpp/classes/viewport_texture.hpp>

#include <godot_cpp/classes/packed_scene.hpp>

#include "host/godot/lfw_godot_renderer.h"

namespace godot {
class Node;
class PackedScene;
}

namespace lfw::host {

class GodotModelBank {
 public:
  explicit GodotModelBank(std::function<bool(const std::string&, std::string&)> load);
  ~GodotModelBank();

  GodotModelBank(const GodotModelBank&) = delete;
  GodotModelBank& operator=(const GodotModelBank&) = delete;

  void set_parent(godot::Node* parent) { _parent = parent; }

  godot::Ref<godot::ViewportTexture> sync(const std::string& tag, const GodotModelSpec& spec);

  void end_frame();

  long long loaded() const { return _loaded; }
  long long failed() const { return _failed; }
  long long live() const { return static_cast<long long>(_slots.size()); }
  const std::string& last_error() const { return _last_error; }
  const std::string& report() const { return _report; }

 private:
  struct Slot;

  godot::Ref<godot::PackedScene> template_for(const std::string& path);
  Slot& ensure_slot(const std::string& tag);
  bool apply_slot(Slot& slot, const GodotModelSpec& spec);

  std::function<bool(const std::string&, std::string&)> _load;
  godot::Node* _parent = nullptr;
  std::map<std::string, godot::Ref<godot::PackedScene>> _templates;
  std::map<std::string, std::unique_ptr<Slot>> _slots;
  long long _loaded = 0;
  long long _failed = 0;
  std::string _last_error;
  std::string _report;
};

}
