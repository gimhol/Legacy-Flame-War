#pragma once

#include <map>
#include <string>

#include "lfw/core/value.h"

namespace lfw {

class I18N {
 public:
  static constexpr const char16_t* kTag = u"I18N";

  I18N();

  const std::u16string& lang() const { return _lang; }

  bool set_lang(const Value& lang, std::u16string& error);

  void add(const Value& langs);

  Value alias(const Value& lang) const;
  Value canonical(const Value& lang) const;
  Value string(const Value& name, const Value& lang) const;
  Value strings(const Value& name, const Value& lang) const;

 private:
  const Object* words_of(const Value& lang) const;
  const Object* lists_of(const Value& lang) const;
  const std::u16string* alias_of(const std::u16string& lang) const;

  std::map<std::u16string, Value> _words;
  std::map<std::u16string, Value> _lists;
  std::map<std::u16string, std::u16string> _alias_map;
  std::u16string _lang;
};

}
