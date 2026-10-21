#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "lfw/base/graves.h"
#include "lfw/controller/base_controller.h"
#include "lfw/core/value.h"
#include "lfw/data/entity_data.h"
#include "lfw/data/ui_info.h"

namespace lfw {

namespace buff {
class Buff;
}
namespace state {
class States;
}
class Entity;
class LFW;
class World;

using FactoryKey = Value;

using IEntityCreators = std::function<Entity*(World*, const EntityData&, state::States*)>;

class ICtrlCreator {
 public:
  virtual ~ICtrlCreator() = default;
  virtual controller::BaseController* create(const std::u16string& player_id,
                                             Entity* entity) const = 0;
};

class IBuffCreator {
 public:
  virtual ~IBuffCreator() = default;
  virtual const Value& kind() const = 0;
  virtual const std::vector<Value>& groups() const = 0;
  virtual buff::Buff* create(LFW* lfw, const std::u16string& id, const Value& kind) const = 0;
};

namespace ui {
class UIComponent;
class UINode;
}

class IComponentCreator {
 public:
  virtual ~IComponentCreator() = default;
  virtual ui::UIComponent* create(ui::UINode& layout, const std::u16string& f_name,
                                  const Value& info) const = 0;
};

using FactoryWarn = std::function<void(const std::u16string&)>;

class Factory {
 public:
  static constexpr const char* TAG = "Factory";

  static void set_warn(FactoryWarn warn);
  static void warn(const std::u16string& text);

  static std::vector<std::pair<FactoryKey, IEntityCreators>>& entity_creators();
  static std::vector<std::pair<FactoryKey, const ICtrlCreator*>>& ctrl_creators();
  static std::vector<std::pair<FactoryKey, const IBuffCreator*>>& buff_creators();
  static std::vector<std::pair<Value, std::vector<FactoryKey>>>& buff_groups();
  static std::vector<std::pair<std::u16string, const IComponentCreator*>>& components();

  static void register_entity(const FactoryKey& type, const IEntityCreators& creator);
  static void register_ctrl(const FactoryKey& oid, const ICtrlCreator* creator);
  static void register_buff(const IBuffCreator* creator);
  static void register_component(const std::u16string& name, const IComponentCreator* creator);

  std::vector<std::pair<FactoryKey, Graves<Entity*>>> graves_maps;
  std::vector<std::pair<FactoryKey, Graves<buff::Buff*>>> buff_graves_maps;
  std::vector<std::pair<const ICtrlCreator*, Graves<controller::BaseController*>>> ctrl_graves_maps;

  buff::Buff* create_buff(const FactoryKey& kind, LFW* lfw, const std::u16string& id);
  Factory& recycle_buff(buff::Buff* buff);
  Factory& recycle_entity(Entity* e);
  Entity* acquire_entity(const FactoryKey& type);
  Entity* create_entity(World* world, const EntityData& data, state::States* states);
  controller::BaseController* create_ctrl(const FactoryKey& oid, const std::u16string& player_id,
                                          Entity* entity);
  controller::BaseController* acquire_ctrl(const ICtrlCreator* cls,
                                           const std::u16string& player_id, Entity* entity);
  Factory& release_ctrl(controller::BaseController* ctrl);
  Entity* create_entity_with_bot(const std::u16string& player_id, World* world,
                                 const EntityData& data,
                                 state::States* states);
  Entity* create_entity_with_player(const std::u16string& player_id, World* world,
                                    const EntityData& data, state::States* states);
  std::vector<std::unique_ptr<ui::UIComponent>> create_components(
      ui::UINode& layout, const std::vector<ComponentInfo>& components);
};

}
