#include "lfw/loader/preprocess_next_frame.h"

#include <unordered_map>
#include <variant>

#include "lfw/core/value.h"
#include "lfw/entity/entity.h"
#include "lfw/loader/get_val_from_entity.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace loader {

namespace {

bool is_nullish(const Value& v) {
  return std::holds_alternative<std::monostate>(v) || std::holds_alternative<NullTag>(v);
}

struct Judger {
  std::u16string source;
  Expression<Entity> expr;
};

std::unordered_map<const Object*, Judger>& judger_registry() {
  static std::unordered_map<const Object*, Judger> registry;
  return registry;
}

}

bool preprocess_next_frame(Value& nf) {
  const Array* a = as_array(nf);
  if (a != nullptr) {
    const size_t n = a->size();
    for (size_t i = 0; i < n; ++i) {
      Value item = a->at(i);
      if (!preprocess_next_frame(item)) return false;
    }
    return true;
  }
  if (is_nullish(nf)) return false;
  const Object* const object = as_object(nf);
  if (object != nullptr) {
    const Value* const src = object->get(u"expression");
    if (src != nullptr) {
      const std::u16string* const text = std::get_if<std::u16string>(src);
      if (text != nullptr) {
        judger_registry().insert_or_assign(
            object, Judger{*text, Expression<Entity>(*text, &get_val_getter_from_entity)});
      }
    }
  }
  return true;
}

Expression<Entity>* judger_of(const Value& nf) {
  const Object* const object = as_object(nf);
  if (object == nullptr) return nullptr;
  const auto it = judger_registry().find(object);
  if (it == judger_registry().end()) return nullptr;
  const Value& src = field_or(nf, u"expression");
  const std::u16string* const text = std::get_if<std::u16string>(&src);
  if (text == nullptr || *text != it->second.source) return nullptr;
  return &it->second.expr;
}

}
}
