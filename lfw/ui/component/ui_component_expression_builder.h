#pragma once

#include <string>
#include <vector>

namespace lfw {
namespace ui {

class UIComponentExpressionBuilder {
 public:
  UIComponentExpressionBuilder(const std::u16string& tag,
                               const std::vector<std::u16string>& args);

  UIComponentExpressionBuilder& set_enabled(bool v);
  UIComponentExpressionBuilder& set_id(const std::u16string& id);
  std::u16string to_string() const { return done(); }
  std::u16string done() const;

 private:
  std::u16string _id;
  std::u16string _enabled;
  std::u16string _tag;
  std::u16string _args;
};

}
}
