#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/utils/schema/validate_schema.h"

namespace lfw {
namespace ui {

class UIComponent;

class UIProps {
 public:
  UIProps(const Value& raw_in, UIComponent& owner);
  UIProps(const UIProps&) = delete;
  UIProps& operator=(const UIProps&) = delete;

  const Value& raw() const { return _raw; }
  UIComponent& owner() { return *_owner; }
  const UIComponent& owner() const { return *_owner; }
  schema::SchemaValidator& validator() { return _validator; }
  const std::vector<std::u16string>& errors() const { return _validator.errors(); }
  bool has(const std::u16string& name) const;

  std::optional<double> num(const std::u16string& name) const;
  void set_num(const std::u16string& name, const Value& v);
  void set_str(const std::u16string& name, const Value& v);
  std::optional<std::u16string> str(const std::u16string& name,
                                    const std::vector<std::u16string>& one_of = {}) const;
  void set_strs(const std::u16string& name, std::vector<std::u16string> v);
  std::optional<std::vector<std::u16string>> strs(const std::u16string& name) const;
  std::optional<bool> bool_(const std::u16string& name) const;
  std::optional<std::vector<Value>> nums(const std::u16string& name, double len,
                                         const Value& fallbacks = Value()) const;
  bool validate(const std::u16string& tag, const Value& props_meta);

  Value prop_value(const std::u16string& name) const;
  Value prop_instance(const std::u16string& name);

 private:
  void any_str_arr(const Value& v, std::vector<std::u16string>& out) const;

  Value _raw;
  UIComponent* _owner = nullptr;
  schema::SchemaValidator _validator;
};

}
}
