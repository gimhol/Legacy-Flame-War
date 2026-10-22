#include "host/godot/lfw_godot_models.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <utility>

#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_mixer.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/gltf_document.hpp>
#include <godot_cpp/classes/gltf_state.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace lfw::host {

namespace {

godot::PackedByteArray to_bytes(const std::string& raw) {
  godot::PackedByteArray out;
  if (raw.empty()) return out;
  out.resize(static_cast<int64_t>(raw.size()));
  std::memcpy(out.ptrw(), raw.data(), raw.size());
  return out;
}

godot::String base_dir_of(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) return godot::String("res://");
  return godot::String(("res://" + path.substr(0, slash)).c_str());
}

godot::AnimationPlayer* find_player(godot::Node* node) {
  if (node == nullptr) return nullptr;
  godot::AnimationPlayer* const self = godot::Object::cast_to<godot::AnimationPlayer>(node);
  if (self != nullptr) return self;
  for (int32_t i = 0; i < node->get_child_count(); ++i) {
    godot::AnimationPlayer* const hit = find_player(node->get_child(i));
    if (hit != nullptr) return hit;
  }
  return nullptr;
}

void apply_outline_to_meshes(godot::Node* node, bool on, const godot::Color& color, float width_world) {
  if (node == nullptr) return;
  godot::MeshInstance3D* const mi = godot::Object::cast_to<godot::MeshInstance3D>(node);
  if (mi != nullptr) {
    const godot::Ref<godot::Mesh> mesh = mi->get_mesh();
    if (!mesh.is_null()) {
      const godot::Vector3 gs = mi->get_global_transform().basis.get_scale();
      const double k = std::max(std::max(std::fabs(static_cast<double>(gs.x)),
                                         std::fabs(static_cast<double>(gs.y))),
                                1e-4);
      const float grow = static_cast<float>(static_cast<double>(width_world) / k);
      const int32_t surfaces = mesh->get_surface_count();
      for (int32_t i = 0; i < surfaces; ++i) {
        godot::Ref<godot::Material> own = mi->get_surface_override_material(i);
        if (own.is_null()) {
          const godot::Ref<godot::Material> shared = mesh->surface_get_material(i);
          if (shared.is_null()) continue;
          const godot::Ref<godot::Resource> dup = shared->duplicate();
          own = godot::Ref<godot::Material>(godot::Object::cast_to<godot::Material>(dup.ptr()));
          if (own.is_null()) continue;
          mi->set_surface_override_material(i, own);
        }
        if (!on) {
          own->set_next_pass(godot::Ref<godot::Material>());
          continue;
        }
        godot::Ref<godot::StandardMaterial3D> outline;
        outline.instantiate();
        outline->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
        outline->set_albedo(color);
        if (color.a < 1.0f) outline->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA);
        outline->set_cull_mode(godot::BaseMaterial3D::CULL_FRONT);
        outline->set_grow_enabled(true);
        outline->set_grow(grow);
        own->set_next_pass(outline);
      }
    }
  }
  for (int32_t i = 0; i < node->get_child_count(); ++i) {
    apply_outline_to_meshes(node->get_child(i), on, color, width_world);
  }
}

void merge_mesh_aabb(godot::Node* node, godot::AABB& box, bool& first) {
  if (node == nullptr) return;
  godot::MeshInstance3D* const mesh_inst = godot::Object::cast_to<godot::MeshInstance3D>(node);
  if (mesh_inst != nullptr) {
    const godot::Ref<godot::Mesh> mesh = mesh_inst->get_mesh();
    if (!mesh.is_null()) {
      const godot::AABB aabb = mesh->get_aabb();
      if (first) {
        box = aabb;
        first = false;
      } else {
        box = box.merge(aabb);
      }
    }
  }
  for (int32_t i = 0; i < node->get_child_count(); ++i) merge_mesh_aabb(node->get_child(i), box, first);
}

}

struct GodotModelBank::Slot {
  godot::SubViewport* viewport = nullptr;
  godot::Node3D* root = nullptr;
  godot::Camera3D* camera = nullptr;
  godot::AnimationPlayer* player = nullptr;
  godot::Node* instance = nullptr;
  std::string path;
  std::string anim;
  bool loop = false;
  bool reverse = false;
  bool hold_last = false;
  bool seen = false;
  bool outline_on = false;
  double outline_r = 0, outline_g = 0, outline_b = 0, outline_a = 0, outline_w = 0;
  double outline_scale = 1;
  double outline_view_scale = 1;
};

GodotModelBank::GodotModelBank(std::function<bool(const std::string&, std::string&)> load)
    : _load(std::move(load)) {}

GodotModelBank::~GodotModelBank() {
  _slots.clear();
}

godot::Ref<godot::PackedScene> GodotModelBank::template_for(const std::string& path) {
  const auto found = _templates.find(path);
  if (found != _templates.end()) return found->second;

  godot::Ref<godot::PackedScene> packed;
  std::string raw;
  if (!_load(path, raw) || raw.empty()) {
    ++_failed;
    _last_error = "read failed: " + path;
    _templates.emplace(path, godot::Ref<godot::PackedScene>());
    return {};
  }
  godot::Ref<godot::GLTFDocument> doc;
  doc.instantiate();
  godot::Ref<godot::GLTFState> state;
  state.instantiate();
  state->set_base_path(base_dir_of(path));
  state->set_create_animations(true);
  const godot::Error parsed = doc->append_from_buffer(to_bytes(raw), state->get_base_path(), state);
  if (parsed != godot::OK) {
    ++_failed;
    _last_error = "glb parse failed (" + std::to_string(static_cast<int>(parsed)) + "): " + path;
    _templates.emplace(path, godot::Ref<godot::PackedScene>());
    return {};
  }
  godot::Node* const scene = doc->generate_scene(state);
  if (scene == nullptr) {
    ++_failed;
    _last_error = "glb scene failed: " + path;
    _templates.emplace(path, godot::Ref<godot::PackedScene>());
    return {};
  }
  godot::AABB box;
  bool first_box = true;
  merge_mesh_aabb(scene, box, first_box);
  packed.instantiate();
  const godot::Error packed_ok = packed->pack(scene);
  memdelete(scene);
  if (packed_ok != godot::OK) {
    ++_failed;
    _last_error = "glb pack failed (" + std::to_string(static_cast<int>(packed_ok)) + "): " + path;
    packed = godot::Ref<godot::PackedScene>();
  } else {
    ++_loaded;
    char info[192];
    std::snprintf(info, sizeof(info), "(anims=%d,box=%.2fx%.2fx%.2f@%.2f,%.2f,%.2f)",
                  static_cast<int>(state->get_animations().size()),
                  static_cast<double>(box.size.x), static_cast<double>(box.size.y),
                  static_cast<double>(box.size.z), static_cast<double>(box.position.x),
                  static_cast<double>(box.position.y), static_cast<double>(box.position.z));
    _report += path + info + " ";
  }
  _templates.emplace(path, packed);
  return packed;
}

GodotModelBank::Slot& GodotModelBank::ensure_slot(const std::string& tag) {
  const auto found = _slots.find(tag);
  if (found != _slots.end()) return *found->second;

  auto slot = std::make_unique<Slot>();
  slot->viewport = memnew(godot::SubViewport);
  slot->viewport->set_size(godot::Vector2i(static_cast<int32_t>(kModelViewport),
                                           static_cast<int32_t>(kModelViewport)));
  slot->viewport->set_transparent_background(true);
  slot->viewport->set_update_mode(godot::SubViewport::UPDATE_ALWAYS);
  slot->viewport->set_use_own_world_3d(true);
  if (_parent != nullptr) _parent->add_child(slot->viewport);

  slot->camera = memnew(godot::Camera3D);
  slot->camera->set_projection(godot::Camera3D::PROJECTION_ORTHOGONAL);
  slot->camera->set_orthogonal(static_cast<float>(kModelViewport), 0.05f,
                               static_cast<float>(kModelViewport * 8.0));
  slot->camera->set_position(godot::Vector3(0.0f, 0.0f, static_cast<float>(kModelViewport)));
  slot->viewport->add_child(slot->camera);
  slot->camera->set_current(true);

  slot->root = memnew(godot::Node3D);
  slot->viewport->add_child(slot->root);

  godot::Ref<godot::Environment> env;
  env.instantiate();
  env->set_background(godot::Environment::BG_CLEAR_COLOR);
  env->set_ambient_source(godot::Environment::AMBIENT_SOURCE_COLOR);
  env->set_ambient_light_color(godot::Color(1.0f, 1.0f, 1.0f));
  env->set_ambient_light_energy(static_cast<float>(0.7 / 3.14159265358979323846));
  godot::WorldEnvironment* const world_env = memnew(godot::WorldEnvironment);
  world_env->set_environment(env);
  slot->viewport->add_child(world_env);

  godot::DirectionalLight3D* const light = memnew(godot::DirectionalLight3D);
  light->set_param(godot::Light3D::PARAM_ENERGY,
                   static_cast<float>(1.4 / 3.14159265358979323846));
  slot->viewport->add_child(light);
  light->look_at_from_position(godot::Vector3(-300.0f, 400.0f, 600.0f), godot::Vector3(),
                               godot::Vector3(0.0f, 1.0f, 0.0f));

  Slot& out = *slot;
  _slots.emplace(tag, std::move(slot));
  return out;
}

bool GodotModelBank::apply_slot(Slot& slot, const GodotModelSpec& spec) {
  if (slot.path != spec.path) {
    const godot::Ref<godot::PackedScene> packed = template_for(spec.path);
    if (packed.is_null()) return false;
    if (slot.instance != nullptr) {
      slot.root->remove_child(slot.instance);
      slot.instance->queue_free();
      slot.instance = nullptr;
    }
    slot.player = nullptr;
    slot.anim.clear();
    slot.instance = packed->instantiate();
    if (slot.instance == nullptr) {
      _last_error = "glb instantiate failed: " + spec.path;
      return false;
    }
    slot.root->add_child(slot.instance);
    slot.path = spec.path;
    slot.player = find_player(slot.instance);
  }
  slot.root->set_position(godot::Vector3(static_cast<float>(spec.px), static_cast<float>(spec.py),
                                         static_cast<float>(spec.pz)));
  slot.root->set_scale(godot::Vector3(static_cast<float>(spec.sx), static_cast<float>(spec.sy),
                                      static_cast<float>(spec.sz)));
  const double cx = std::cos(spec.rx / 2.0), cy = std::cos(spec.ry / 2.0);
  const double cz = std::cos(spec.rz / 2.0);
  const double sx = std::sin(spec.rx / 2.0), sy = std::sin(spec.ry / 2.0);
  const double sz = std::sin(spec.rz / 2.0);
  slot.root->set_quaternion(godot::Quaternion(
      static_cast<float>(sx * cy * cz + cx * sy * sz),
      static_cast<float>(cx * sy * cz - sx * cy * sz),
      static_cast<float>(cx * cy * sz + sx * sy * cz),
      static_cast<float>(cx * cy * cz - sx * sy * sz)));

  if (slot.player != nullptr) {
    const godot::StringName clip(spec.anim.c_str());
    if (!spec.anim.empty() && slot.player->has_animation(clip)) {
      if (slot.anim != spec.anim || slot.loop != spec.loop || slot.reverse != spec.reverse) {
        godot::Ref<godot::Animation> animation = slot.player->get_animation(clip);
        if (!animation.is_null()) {
          animation->set_loop_mode(spec.loop ? godot::Animation::LOOP_LINEAR
                                            : godot::Animation::LOOP_NONE);
        }
        slot.anim = spec.anim;
        slot.loop = spec.loop;
        slot.reverse = spec.reverse;
        slot.hold_last = spec.hold_last;
        slot.player->set_speed_scale(spec.reverse ? -1.0f : 1.0f);
        slot.player->play(clip);
      }
    } else if (!slot.anim.empty()) {
      slot.player->stop();
      slot.anim.clear();
    }
  }
  const bool outline_on = spec.outline_w > 0.0 && spec.outline_a > 0.0;
  const double k = std::max(std::max(std::fabs(spec.sx), std::fabs(spec.sy)), 1e-6);
  const double view_scale = spec.view_scale > 0.0 ? spec.view_scale : 1.0;
  if (outline_on != slot.outline_on || spec.outline_r != slot.outline_r ||
      spec.outline_g != slot.outline_g || spec.outline_b != slot.outline_b ||
      spec.outline_a != slot.outline_a || spec.outline_w != slot.outline_w ||
      k != slot.outline_scale || view_scale != slot.outline_view_scale) {
    slot.outline_on = outline_on;
    slot.outline_r = spec.outline_r;
    slot.outline_g = spec.outline_g;
    slot.outline_b = spec.outline_b;
    slot.outline_a = spec.outline_a;
    slot.outline_w = spec.outline_w;
    slot.outline_scale = k;
    slot.outline_view_scale = view_scale;
    const godot::Color color(static_cast<float>(spec.outline_r), static_cast<float>(spec.outline_g),
                             static_cast<float>(spec.outline_b),
                             static_cast<float>(spec.outline_a));
    // 2D 的描边宽度是画布像素；3D 的世界单位换算 = 画布像素 / 每单位像素数
    apply_outline_to_meshes(slot.instance, outline_on, color,
                            static_cast<float>(spec.outline_w / view_scale));
  }
  return true;
}

godot::Ref<godot::ViewportTexture> GodotModelBank::sync(const std::string& tag,
                                                       const GodotModelSpec& spec) {
  Slot& slot = ensure_slot(tag);
  slot.seen = true;
  if (!apply_slot(slot, spec)) {
    slot.viewport->set_update_mode(godot::SubViewport::UPDATE_DISABLED);
    return {};
  }
  slot.viewport->set_update_mode(godot::SubViewport::UPDATE_ALWAYS);
  return slot.viewport->get_texture();
}

void GodotModelBank::end_frame() {
  for (auto& entry : _slots) {
    if (!entry.second->seen) {
      entry.second->viewport->set_update_mode(godot::SubViewport::UPDATE_DISABLED);
    }
    entry.second->seen = false;
  }
}

}
