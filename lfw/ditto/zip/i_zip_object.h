#pragma once

#include <string>

#include "lfw/core/value.h"

namespace lfw {

class IZipObject {
 public:
  virtual ~IZipObject() = default;
  virtual const std::u16string& name() const = 0;
  virtual bool json(Value& out, std::u16string& error) = 0;
  virtual bool text(Value& out, std::u16string& error) = 0;
  virtual bool blob_url(Value& out, std::u16string& error) = 0;
  virtual bool array_buffer(Value& out, std::u16string& error) = 0;
  virtual bool image_bitmap(Value& out, std::u16string& error) = 0;
};

}
