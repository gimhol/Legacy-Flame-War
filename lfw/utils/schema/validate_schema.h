#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {
namespace schema {

inline bool is_class_type(const Value& type) {
  const std::u16string* s = std::get_if<std::u16string>(&type);
  return s != nullptr && s->rfind(u"$cls:", 0) == 0;
}

inline std::u16string class_type_name(const Value& type) {
  const std::u16string* s = std::get_if<std::u16string>(&type);
  return s != nullptr && s->rfind(u"$cls:", 0) == 0 ? s->substr(5) : std::u16string();
}

class SchemaValidator {
 public:
  static SchemaValidator& Default();

  using InstanceGetter =
      std::function<Value(const Value& raw_value, const std::u16string& clazz,
                          const Value& prop_schema)>;
  using InstanceSetter =
      std::function<void(const Value& value, const Value& raw_value, const std::u16string& clazz,
                         const Value& prop_schema)>;

  SchemaValidator& instance_getter(InstanceGetter func);
  SchemaValidator& instance_setter(InstanceSetter func);
  bool has_instance_getter() const { return static_cast<bool>(_get_instance); }
  bool has_instance_setter() const { return static_cast<bool>(_set_instance); }

  struct DefinedInstance {
    enum class Kind { ObjectProp, ArrayItem };
    Kind kind = Kind::ObjectProp;
    Value target;
    std::u16string key;
    Value raw_value;
    Value schema;
  };
  const std::vector<DefinedInstance>& defined_instances() const { return _defined; }
  void clear_defined_instances() { _defined.clear(); }

  struct InstanceAccess {
    bool ok = false;
    Value value;
    std::u16string error;
  };
  InstanceAccess get_instance(size_t index) const;
  std::u16string set_instance(size_t index, const Value& v) const;

  const std::vector<std::u16string>& errors() const { return _errors; }
  const std::vector<std::u16string>& warnings() const { return _warnings; }

  bool validate(const Value& value, const Value& schema);
  void reset();

 private:
  InstanceGetter _get_instance;
  InstanceSetter _set_instance;
  std::vector<DefinedInstance> _defined;
  std::vector<std::u16string> _errors;
  std::vector<std::u16string> _warnings;
};

}
}
