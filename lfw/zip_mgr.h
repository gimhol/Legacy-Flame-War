#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "lfw/defines/i_data_info.h"
#include "lfw/ditto/zip/i_zip.h"
#include "lfw/ditto/zip/i_zip_object.h"

namespace lfw {

struct IZipResult {
  std::u16string origin;
  IZipObject* file = nullptr;
  IZip* zip = nullptr;
};

struct ILoadedZip {
  IZip* zip = nullptr;
  IDataInfo* info = nullptr;
};

class ZipMgr {
 public:
  size_t length() const { return _list.size(); }
  const std::vector<ILoadedZip>& all() const { return _list; }
  std::vector<IZip*> zips() const;
  std::vector<Value> md5s() const;
  std::vector<IDataInfo*> data_infos() const;
  void add(const ILoadedZip& zip);
  void clear();
  std::vector<IZipResult> find(const std::vector<std::u16string>& paths, bool exact);

 private:
  std::vector<ILoadedZip> _list;
};

}
