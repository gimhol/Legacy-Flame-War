#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

struct XmlAttr {
  std::u16string name;
  std::u16string value;
};

class IXMLElement {
 public:
  virtual ~IXMLElement() = default;

  virtual const std::u16string& tag() const = 0;
  virtual const std::vector<IXMLElement*>& children() const = 0;
  virtual const std::vector<XmlAttr>& attrs() const = 0;
  virtual const std::u16string& text() const = 0;
  virtual IXMLElement* parent() const = 0;

  virtual std::optional<std::u16string> attr(const std::u16string& name) const = 0;
  virtual std::optional<std::u16string> str_attr(const std::u16string& name) const = 0;
  virtual std::optional<double> num_attr(const std::u16string& name) const = 0;
  virtual std::optional<bool> bool_attr(const std::u16string& name) const = 0;
  virtual std::optional<std::vector<std::u16string>> strs_attr(const std::u16string& name,
                                                               char16_t sep = u',') const = 0;
  virtual std::optional<std::vector<double>> nums_attr(const std::u16string& name,
                                                       char16_t sep = u',') const = 0;
  virtual std::optional<std::vector<std::optional<std::u16string>>> strs_attr_soft(
      const std::u16string& name, char16_t sep = u',') const = 0;
  virtual std::optional<std::vector<std::optional<double>>> nums_attr_soft(
      const std::u16string& name, char16_t sep = u',') const = 0;

  virtual void set_attr(const std::u16string& name, const Value& value, char16_t sep = u',') = 0;
  virtual void del_attr(const std::u16string& name) = 0;
  virtual void set_arr_attr_soft(const std::u16string& name, const Value& value,
                                 char16_t sep = u',') = 0;
  virtual bool has_attr(const std::u16string& name) const = 0;
  virtual void set_text(const std::u16string& text) = 0;

  virtual Value as_value() const = 0;
  virtual std::optional<std::u16string> as_string() const = 0;
  virtual std::u16string as_string(const std::u16string& or_value) const = 0;
  virtual std::optional<double> as_number() const = 0;
  virtual double as_number(double or_value) const = 0;
  virtual std::optional<bool> as_boolean() const = 0;
  virtual bool as_boolean(bool or_value) const = 0;
  virtual Value as_array() const = 0;
  virtual Value as_array(const Value& or_value) const = 0;
  virtual Value as_object() const = 0;
  virtual Value as_object(const Value& or_value) const = 0;

  virtual std::u16string action_str() const = 0;
  virtual std::u16string stringify() const = 0;

  virtual void insert(const std::shared_ptr<IXMLElement>& child,
                      std::optional<std::size_t> index = std::nullopt) = 0;
  virtual bool remove(IXMLElement* child) = 0;
  virtual bool remove_self() = 0;
  virtual void remove_all() = 0;

  virtual std::vector<IXMLElement*> children_by_tag(const std::u16string& tag) const = 0;
  virtual IXMLElement* child_by_tag(const std::u16string& tag) const = 0;

  virtual std::optional<std::u16string> get_str(const std::u16string& name) const = 0;
  virtual std::u16string get_str(const std::u16string& name,
                                 const std::u16string& or_value) const = 0;
  virtual std::optional<double> get_num(const std::u16string& name) const = 0;
  virtual double get_num(const std::u16string& name, double or_value) const = 0;
  virtual std::optional<bool> get_bool(const std::u16string& name) const = 0;
  virtual bool get_bool(const std::u16string& name, bool or_value) const = 0;
  virtual std::optional<std::vector<std::u16string>> get_str_arr(
      const std::u16string& name) const = 0;
  virtual std::optional<std::vector<double>> get_num_arr(const std::u16string& name) const = 0;
  virtual Value get_obj(const std::u16string& tag) const = 0;
};

}
