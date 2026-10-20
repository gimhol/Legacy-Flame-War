#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/ditto/xml/i_xml_element.h"

namespace lfw {

class ToolXMLElement : public IXMLElement {
 public:
  explicit ToolXMLElement(std::u16string tag);

  std::optional<std::u16string> type() const;

  const std::u16string& tag() const override { return tag_; }
  const std::vector<IXMLElement*>& children() const override { return child_views_; }
  const std::vector<XmlAttr>& attrs() const override { return attrs_; }
  const std::u16string& text() const override { return text_; }
  IXMLElement* parent() const override { return parent_; }

  std::optional<std::u16string> attr(const std::u16string& name) const override;
  std::optional<std::u16string> str_attr(const std::u16string& name) const override;
  std::optional<double> num_attr(const std::u16string& name) const override;
  std::optional<bool> bool_attr(const std::u16string& name) const override;
  std::optional<std::vector<std::u16string>> strs_attr(const std::u16string& name,
                                                       char16_t sep = u',') const override;
  std::optional<std::vector<double>> nums_attr(const std::u16string& name,
                                               char16_t sep = u',') const override;
  std::optional<std::vector<std::optional<std::u16string>>> strs_attr_soft(
      const std::u16string& name, char16_t sep = u',') const override;
  std::optional<std::vector<std::optional<double>>> nums_attr_soft(const std::u16string& name,
                                                                   char16_t sep = u',') const override;

  void set_attr(const std::u16string& name, const Value& value, char16_t sep = u',') override;
  void del_attr(const std::u16string& name) override;
  void set_arr_attr_soft(const std::u16string& name, const Value& value,
                         char16_t sep = u',') override;
  bool has_attr(const std::u16string& name) const override;
  void set_text(const std::u16string& text) override { text_ = text; }

  Value as_value() const override;
  std::optional<std::u16string> as_string() const override;
  std::u16string as_string(const std::u16string& or_value) const override;
  std::optional<double> as_number() const override;
  double as_number(double or_value) const override;
  std::optional<bool> as_boolean() const override;
  bool as_boolean(bool or_value) const override;
  Value as_array() const override;
  Value as_array(const Value& or_value) const override;
  Value as_object() const override;
  Value as_object(const Value& or_value) const override;

  std::u16string action_str() const override;
  std::u16string stringify() const override;

  void insert(const std::shared_ptr<IXMLElement>& child,
              std::optional<std::size_t> index) override;
  bool remove(IXMLElement* child) override;
  bool remove_self() override;
  void remove_all() override;

  std::vector<IXMLElement*> children_by_tag(const std::u16string& tag) const override;
  IXMLElement* child_by_tag(const std::u16string& tag) const override;

  std::optional<std::u16string> get_str(const std::u16string& name) const override;
  std::u16string get_str(const std::u16string& name,
                         const std::u16string& or_value) const override;
  std::optional<double> get_num(const std::u16string& name) const override;
  double get_num(const std::u16string& name, double or_value) const override;
  std::optional<bool> get_bool(const std::u16string& name) const override;
  bool get_bool(const std::u16string& name, bool or_value) const override;
  std::optional<std::vector<std::u16string>> get_str_arr(const std::u16string& name) const override;
  std::optional<std::vector<double>> get_num_arr(const std::u16string& name) const override;
  Value get_obj(const std::u16string& tag) const override;

  void add_child(const std::shared_ptr<IXMLElement>& child);

 private:
  std::u16string tag_;
  std::u16string text_;
  std::vector<XmlAttr> attrs_;
  std::vector<std::shared_ptr<IXMLElement>> child_owners_;
  std::vector<IXMLElement*> child_views_;
  IXMLElement* parent_ = nullptr;
};

}
