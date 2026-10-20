#pragma once

#include <string>
#include <vector>

#include "lfw/core/value.h"

namespace lfw {


enum class DebugLevel { Debug, Warn, Log };

class IDebugging {
 public:
  virtual ~IDebugging() = default;

  virtual bool debugging_enabled() const = 0;
  virtual const std::u16string& debugging_id() const = 0;
  virtual const std::u16string& debugging_tag() const = 0;
  virtual void debugging_out(DebugLevel level, const std::vector<Value>& args) = 0;

  virtual void debug(const std::u16string& func, const std::vector<Value>& args = {}) = 0;
  virtual void warn(const std::u16string& func, const std::vector<Value>& args = {}) = 0;
  virtual void log(const std::u16string& func, const std::vector<Value>& args = {}) = 0;

  bool debugging_attached = false;
};

inline void make_debugging(IDebugging& obj) { obj.debugging_attached = true; }

void debugging_write(IDebugging& obj, DebugLevel level, const std::u16string& func,
                     const std::vector<Value>& args);

}
