#include "lfw/i18n.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {

namespace {

bool is_undefined(const Value& v) { return std::holds_alternative<std::monostate>(v); }

bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool loose_empty(const Value& v) { return equals(v, Value(std::u16string())); }

std::u16string join_lines(const Array& a) {
  std::u16string out;
  for (size_t i = 0; i < a.size(); ++i) {
    if (i != 0) out.push_back(u'\n');
    const Value& item = a.at(i);
    if (is_nullish(item)) continue;
    out += to_string(item);
  }
  return out;
}

Value empty_object() { return Value(std::make_shared<Object>(Object())); }

Value fallback_list(const Value& name) {
  Array one;
  one.push_back(name);
  return Value(std::make_shared<Array>(one));
}

Value key_at(const Object* const obj, const Value& name) {
  if (obj == nullptr) return Value();
  const Value* const found = obj->get(to_string(name));
  return found != nullptr ? *found : Value();
}

}

I18N::I18N() {
  _words[u""] = empty_object();
  _lists[u""] = empty_object();
}

const Object* I18N::words_of(const Value& lang) const {
  const std::u16string* const k = std::get_if<std::u16string>(&lang);
  if (k == nullptr) return nullptr;
  const auto it = _words.find(*k);
  return it == _words.end() ? nullptr : as_object(it->second);
}

const Object* I18N::lists_of(const Value& lang) const {
  const std::u16string* const k = std::get_if<std::u16string>(&lang);
  if (k == nullptr) return nullptr;
  const auto it = _lists.find(*k);
  return it == _lists.end() ? nullptr : as_object(it->second);
}

const std::u16string* I18N::alias_of(const std::u16string& lang) const {
  const auto it = _alias_map.find(lang);
  return it == _alias_map.end() ? nullptr : &it->second;
}

bool I18N::set_lang(const Value& lang, std::u16string& error) {
  const std::u16string* const text = std::get_if<std::u16string>(&lang);
  if (text == nullptr) {
    error = u"[" + std::u16string(kTag) + u"::set_lang] lang should be string, but got " +
            to_string(lang);
    return false;
  }
  _lang = *text;
  return true;
}

void I18N::add(const Value& langs) {
  if (!truthy(langs)) return;
  const Object* const root = as_object(langs);
  if (root == nullptr) return;

  const std::vector<std::u16string> lang_names = root->keys();
  for (const std::u16string& lang_name : lang_names) {
    const Value* const found = root->get(lang_name);
    const Value new_words = found != nullptr ? *found : Value();
    if (const std::u16string* const alias = std::get_if<std::u16string>(&new_words)) {
      _alias_map[lang_name] = *alias;
      continue;
    }
    if (!truthy(new_words)) continue;
    const Object* const word_map = as_object(new_words);
    if (word_map == nullptr) continue;

    auto words_it = _words.find(lang_name);
    if (words_it == _words.end()) words_it = _words.emplace(lang_name, empty_object()).first;
    auto lists_it = _lists.find(lang_name);
    if (lists_it == _lists.end()) lists_it = _lists.emplace(lang_name, empty_object()).first;
    Object* const strings = as_object(words_it->second);
    Object* const lists = as_object(lists_it->second);

    const std::vector<std::u16string> keys = word_map->keys();
    for (const std::u16string& k : keys) {
      const Value* const word_ptr = word_map->get(k);
      const Value word = word_ptr != nullptr ? *word_ptr : Value();
      if (const std::u16string* const text = std::get_if<std::u16string>(&word)) {
        strings->set(k, Value(*text));
        Array one;
        one.push_back(Value(*text));
        lists->set(k, Value(std::make_shared<Array>(one)));
        continue;
      }
      const Array* const arr = as_array(word);
      if (arr == nullptr) continue;
      Array items;
      for (size_t i = 0; i < arr->size(); ++i) items.push_back(Value(to_string(arr->at(i))));
      strings->set(k, Value(join_lines(*arr)));
      lists->set(k, Value(std::make_shared<Array>(items)));
    }
  }
}

Value I18N::alias(const Value& lang) const {
  Value ret = lang;
  std::vector<Value> visited;
  while (!is_undefined(ret)) {
    for (const Value& seen : visited) {
      if (strict_equals(seen, ret)) return Value();
    }
    visited.push_back(ret);
    const std::u16string* const key = std::get_if<std::u16string>(&ret);
    const std::u16string* const next = key != nullptr ? alias_of(*key) : nullptr;
    if (next == nullptr || next->empty()) break;
    ret = Value(*next);
  }
  if (strict_equals(lang, ret)) return Value();
  return ret;
}

Value I18N::canonical(const Value& lang) const {
  const Value got = alias(lang);
  return is_nullish(got) ? lang : got;
}

Value I18N::string(const Value& name, const Value& lang) const {
  const Object* const words = words_of(lang);
  if (loose_empty(lang)) {
    const Value got = key_at(words, name);
    return is_nullish(got) ? name : got;
  }
  const Value got = key_at(words, name);
  if (!is_nullish(got)) return got;
  const Value a = alias(lang);
  return string(name, is_nullish(a) ? Value(std::u16string()) : a);
}

Value I18N::strings(const Value& name, const Value& lang) const {
  const Object* const lists = lists_of(lang);
  if (loose_empty(lang)) {
    const Value got = key_at(lists, name);
    return is_nullish(got) ? fallback_list(name) : got;
  }
  const Value got = key_at(lists, name);
  if (!is_nullish(got)) return got;
  const Value a = alias(lang);
  return strings(name, is_nullish(a) ? Value(std::u16string()) : a);
}

}
