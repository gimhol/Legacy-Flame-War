#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lfw/ditto/zip/i_zip.h"

namespace lfw::host {

std::string md5_hex(const uint8_t* data, size_t size);
std::string md5_hex(const std::string& text);

bool inflate_bytes(const uint8_t* data, size_t size, std::vector<uint8_t>& out,
                   std::u16string& error);

class ZipFile;

class ZipFileObject : public IZipObject {
 public:
  ZipFileObject(ZipFile& owner, size_t index, std::u16string name);

  const std::u16string& name() const override { return name_; }
  bool json(Value& out, std::u16string& error) override;
  bool text(Value& out, std::u16string& error) override;
  bool blob_url(Value& out, std::u16string& error) override;
  bool array_buffer(Value& out, std::u16string& error) override;
  bool image_bitmap(Value& out, std::u16string& error) override;

  size_t index() const { return index_; }
  bool read(std::string& out, std::u16string& error);

 private:
  ZipFile* owner_;
  size_t index_;
  std::u16string name_;
  std::optional<std::string> cache_;
};

class ZipFile : public IZip {
 public:
  static bool open(std::u16string name, std::shared_ptr<const std::vector<uint8_t>> data,
                   const std::optional<std::u16string>& md5, std::unique_ptr<ZipFile>& out,
                   std::u16string& error);
  static bool open(std::u16string name, std::vector<uint8_t> data,
                   const std::optional<std::u16string>& md5, std::unique_ptr<ZipFile>& out,
                   std::u16string& error);

  const std::u16string& name() const override { return name_; }
  IZipObject* file(const std::u16string& path) override;
  std::vector<IZipObject*> file_regex(const std::u16string& pattern) override;
  std::optional<std::u16string> md5() const override { return md5_; }

  size_t entry_count() const { return entries_.size(); }
  const std::u16string& entry_name(size_t index) const { return entries_[index].name; }
  uint16_t entry_method(size_t index) const { return entries_[index].method; }
  uint32_t entry_size(size_t index) const { return entries_[index].usize; }
  bool read_entry(size_t index, std::string& out, std::u16string& error);

 private:
  struct Entry {
    std::u16string name;
    uint16_t flags = 0;
    uint16_t method = 0;
    uint32_t crc = 0;
    uint32_t csize = 0;
    uint32_t usize = 0;
    uint32_t lho = 0;
  };

  ZipFile(std::u16string name, std::shared_ptr<const std::vector<uint8_t>> data,
          std::optional<std::u16string> md5);

  std::u16string name_;
  std::shared_ptr<const std::vector<uint8_t>> data_;
  std::optional<std::u16string> md5_;
  std::vector<Entry> entries_;
  std::vector<std::pair<std::u16string, size_t>> order_;
  std::map<std::u16string, size_t> by_name_;
  std::vector<std::unique_ptr<ZipFileObject>> objects_;
};

}
