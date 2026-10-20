#include "lfw/dat_translator/xml/xml_x_qube.h"

#include <memory>
#include <optional>
#include <vector>

#include "lfw/dat_translator/xml/delete_undefined.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {
namespace dat_translator {
namespace xml {

namespace {

const char16_t* const kKeys[6] = {u"x", u"y", u"w", u"h", u"z", u"l"};

std::optional<double> at(const std::optional<std::vector<std::optional<double>>>& arr,
                         size_t i) {
  if (!arr || i >= arr->size()) return std::nullopt;
  return (*arr)[i];
}

}

Value xml_2_qube(const IXMLElement& el, Value out) {
  if (as_object(out) == nullptr) out = Value(std::make_shared<Object>());
  const std::optional<std::vector<std::optional<double>>> a = el.nums_attr_soft(u"rect");
  const std::optional<std::vector<std::optional<double>>> b = el.nums_attr_soft(u"qube");
  Object* const o = as_object(out);

  Value temp[6];
  for (size_t i = 0; i < 6; ++i) {
    Value fallback = field_or(*o, kKeys[i]);
    if (const std::optional<double> vb = at(b, i)) fallback = Value(*vb);
    if (const std::optional<double> va = at(a, i)) fallback = Value(*va);
    const std::optional<double> got = el.get_num(std::u16string(kKeys[i]));
    temp[i] = got ? Value(*got) : fallback;
  }
  delete_undefined(out);
  for (size_t i = 0; i < 6; ++i) o->set(std::u16string(kKeys[i]), temp[i]);
  return out;
}

}
}
}
