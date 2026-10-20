#pragma once

#include "lfw/factory.h"
#include "lfw/ui/component/ui_component.h"
#include "lfw/ui/register_class.h"

namespace lfw {
namespace ui {

template <typename T>
class ComponentCreatorT : public IComponentCreator {
 public:
  UIComponent* create(UINode& layout, const std::u16string& f_name,
                      const Value& info) const override {
    return new T(layout, f_name, info);
  }
};

void regist_components();

}
}
