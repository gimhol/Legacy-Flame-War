#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lfw/core/value.h"
#include "lfw/ditto/xml/i_xml_element.h"
#include "lfw/zip_mgr.h"

namespace lfw {

struct ImportResult {
  Value data;
  std::shared_ptr<IXMLElement> xml_root;
  Value file;
  Value origin;
};

class IResourcesHost {
 public:
  virtual bool import_as_json(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                              std::u16string& error) = 0;
  virtual bool import_as_blob_url(const std::vector<std::u16string>& urls, Value& data,
                                  Value& hit, std::u16string& error) = 0;
  virtual bool import_as_array_buffer(const std::vector<std::u16string>& urls, Value& data,
                                      Value& hit, std::u16string& error) = 0;
  virtual bool import_as_image_bitmap(const std::vector<std::u16string>& urls, Value& data,
                                      Value& hit, std::u16string& error) = 0;
  virtual bool import_as_text(const std::vector<std::u16string>& urls, Value& data, Value& hit,
                              std::u16string& error) = 0;
  virtual bool xml_parse(const Value& text, Value& marker, std::shared_ptr<IXMLElement>& root,
                         std::u16string& error) = 0;
};

class Resources {
 public:
  static constexpr const char16_t* kTag = u"Resources";

  Resources(ZipMgr* zip_mgr, IResourcesHost* host);

  ZipMgr* zip_mgr() const { return _zip_mgr; }

  bool import_json(const std::u16string& path, bool exact, ImportResult& out,
                   std::u16string& error);
  bool import_resource(const std::u16string& path, bool exact, ImportResult& out,
                       std::u16string& error);
  bool import_image_bitmap(const std::u16string& path, bool exact, ImportResult& out,
                           std::u16string& error);
  bool import_array_buffer(const std::u16string& path, bool exact, ImportResult& out,
                           std::u16string& error);
  bool import_xml(const std::u16string& path, bool exact, ImportResult& out,
                  std::u16string& error);

 private:
  void find_first(const std::vector<std::u16string>& paths, IZipObject** file, Value& origin);

  ZipMgr* _zip_mgr = nullptr;
  IResourcesHost* _host = nullptr;
};

}
