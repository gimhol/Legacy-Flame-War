#include "lfw/data/stage_data.h"

#include <utility>

#include "lfw/core/same_ref.h"
#include "lfw/utils/container_help/field_or.h"

namespace lfw {

StageData::StageData(Value raw) : _raw(std::move(raw)) {
  const Object* const o = as_object(_raw);
  if (o == nullptr) return;
  const Value id_v = field_or(*o, u"id");
  _id = truthy(id_v) ? to_string(id_v) : std::u16string();
  const Value name_v = field_or(*o, u"name");
  _name = truthy(name_v) ? to_string(name_v) : std::u16string();
  _title = field_or(*o, u"title");
  _chapter = field_or(*o, u"chapter");
  _next = field_or(*o, u"next");
  _phases = field_or(*o, u"phases");
  _bg = field_or(*o, u"bg");
}

bool StageData::same_as(const StageData& other) const { return same_ref(_raw, other._raw); }

}
