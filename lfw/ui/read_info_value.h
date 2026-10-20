#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw::ui {


enum class UIJudger {
  None,
  Is0Or1,
  UnsafeIsObject,
  UnsafeIsArray,
  ValidateUIImgInfo,
};

struct UIValueType {
  enum class Kind {
    Null,
    Boolean,
    Number,
    String,
    Judger,
    Clazz,
  };
  Kind kind = Kind::Null;
  UIJudger judger = UIJudger::None;
};

bool judger_run(UIJudger judger, const Value& v);

Value find_ui_value(const Value& ui, const std::u16string& name);

bool parse_ui_value(const Value& ui, const UIValueType& type, const Value& value, Value& out,
                    bool& out_null, std::u16string& error);

}
