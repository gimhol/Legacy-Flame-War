#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ui/component/ui_component.h"

namespace lfw {

class Entity;

namespace ui {

class Picture;
class ScrollView;
class UINode;

class MoveTableLogic : public UIComponent {
 public:
  static constexpr const char* TAG = "MoveTableLogic";
  static const ClazzTag* class_tag();
  static const std::vector<std::u16string>& TAGS();
  static const Value& PROPS();

  MoveTableLogic(UINode& layout, const std::u16string& f_name, const Value& info);

  const ClazzTag* clazz() const override { return class_tag(); }
  const std::u16string& props_tag() const override;
  const Value& props_meta() const override;

  void on_start() override;
  void on_stop() override;
  void on_key_down(LFWKeyEvent& e) override;
  void update(double dt) override;

 private:
  enum class StepKind { Click, Down, Up, Spawn };

  struct KeyStep {
    double at = 0;
    StepKind kind = StepKind::Click;
    std::u16string keys;
    std::optional<size_t> entity;
    Value spec;
  };

  std::vector<Value> lists();
  Value list();

  void bind_rows(UINode& content);
  void load_lists();
  void enter_list(int index);
  void start_move(int index);
  void rebuild_scene(const Value& list, const Value& move);
  void enter_move_frame(const std::u16string& id);
  void set_move_keys(UINode* node, const Value& move);
  void set_move_name(UINode* node, const Value& move, const std::u16string& prefix);
  void update_move_labels(const Value& list, const Value& move);
  void click_row(size_t index);
  void build_steps(const Value& move);
  void push_steps(const Value& seq, std::optional<size_t> entity);
  void exec_step(const KeyStep& step);
  std::vector<std::u16string> parse_keys(const std::u16string& str, const Entity* entity) const;
  Entity* spawn_actor(const Value& list, const Value& move);
  void spawn_entities(const Value& spec);
  double clamp_x(double x) const;
  double calc_entity_fit(const std::vector<Value>& spec) const;
  void spawn_entity(const Value& spec, size_t index);

  int _list_index = 0;
  Value _list_ref;
  int _move_index = 0;
  Entity* _actor = nullptr;
  std::vector<Entity*> _entities;
  std::vector<Value> _entity_specs;
  double _entity_fit = 1;
  std::vector<KeyStep> _steps;
  size_t _step_index = 0;
  double _move_time = 0;
  double _end_wait = 0;
  int _phase = 0;
  bool _loading = false;
  std::function<void()> _load_remover;
  UINode* _char_label = nullptr;
  UINode* _char_prev = nullptr;
  UINode* _char_next = nullptr;
  UINode* _name_label = nullptr;
  UINode* _keys_label = nullptr;
  UINode* _desc_label = nullptr;
  Picture* _face = nullptr;
  ScrollView* _scroll = nullptr;
  bool _rows_bound = false;
  bool _arrows_bound = false;
  std::vector<UINode*> _rows;
  std::vector<UINode*> _row_names;
  std::vector<UINode*> _row_keys;
};

}
}
