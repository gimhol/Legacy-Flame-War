#include "lfw/loader/resolve_prefab.h"

#include <memory>
#include <string>
#include <vector>

#include "lfw/core/js_string.h"
#include "lfw/core/value.h"
#include "lfw/utils/container_help/spread_assign.h"

namespace lfw {
namespace loader {

namespace {

Value field_at(const Value& v, const char16_t* key) {
  const Object* o = as_object(v);
  if (o == nullptr) return Value();
  const Value* p = o->get(std::u16string(key));
  return p != nullptr ? *p : Value();
}

bool is_undefined(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

bool is_empty_ref(const Value& v) {
  const std::u16string* const s = std::get_if<std::u16string>(&v);
  return s != nullptr && s->empty();
}

Value prefab_ref_of(const Value& v) {
  const Value r = field_at(v, u"ref");
  if (!is_undefined(r)) return is_empty_ref(r) ? Value() : r;
  const Value p = field_at(v, u"prefab_id");
  if (is_undefined(p) || is_empty_ref(p)) return Value();
  return p;
}

Value clone_value(const Value& v) {
  if (const Array* const a = as_array(v)) {
    Array out;
    for (const Value& item : a->items()) out.push_back(clone_value(item));
    return Value(std::make_shared<Array>(std::move(out)));
  }
  if (const Object* const o = as_object(v)) {
    Object out;
    for (const std::u16string& k : o->keys()) {
      const Value* const p = o->get(k);
      out.set(k, p != nullptr ? clone_value(*p) : Value());
    }
    return Value(std::make_shared<Object>(std::move(out)));
  }
  return v;
}

bool chain_has(const std::vector<std::u16string>& chain, const std::u16string& ref) {
  for (const std::u16string& c : chain) {
    if (c == ref) return true;
  }
  return false;
}

std::vector<std::u16string> chain_with(const std::vector<std::u16string>& chain,
                                       const std::u16string& ref) {
  std::vector<std::u16string> out = chain;
  out.push_back(ref);
  return out;
}

}

ResolvePrefabResult resolve_prefab(const Value& obj, const Value& prefabs) {
  ResolvePrefabResult res;
  std::vector<std::u16string> chain;
  Value base;
  bool has_base = false;
  Value ref = prefab_ref_of(obj);
  while (!is_undefined(ref)) {
    const std::u16string key = to_string(ref);
    if (chain_has(chain, key)) {
      res.ok = false;
      res.cycle = true;
      res.chain = chain_with(chain, key);
      return res;
    }
    const Value prefab = field_at(prefabs, key.c_str());
    if (!truthy(prefab)) {
      res.ok = false;
      res.cycle = false;
      res.chain = chain_with(chain, key);
      return res;
    }
    chain.push_back(key);
    base = spread_assign(prefab, has_base ? base : Value());
    has_base = true;
    ref = prefab_ref_of(prefab);
  }
  res.ok = true;
  res.chain = chain;
  Value merged = spread_assign(has_base ? base : Value(), obj);
  if (Object* const o = as_object(merged)) {
    o->remove(u"ref");
    o->remove(u"prefab_id");
  }
  res.value = has_base ? clone_value(merged) : merged;
  return res;
}

std::u16string prefab_error_message(const std::u16string& tag, const std::u16string& who,
                                    const std::u16string& what, const ResolvePrefabResult& r) {
  if (!r.cycle) {
    const std::u16string last = r.chain.empty() ? std::u16string() : r.chain[r.chain.size() - 1];
    return std::u16string(u"[") + tag + u"] \"" + who + u"\" " + what +
           u" references missing prefab: \"" + last + u"\"";
  }
  std::u16string joined;
  for (size_t i = 0; i < r.chain.size(); ++i) {
    if (i > 0) joined += u" -> ";
    joined += r.chain[i];
  }
  return std::u16string(u"[") + tag + u"] \"" + who + u"\" " + what +
         u" prefab ref cycle: " + joined;
}

}
}
