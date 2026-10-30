#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>

namespace lfw::host {

/**
 * 远端数据包落盘缓存：`<dir>/<md5>.zip` + `<dir>/cache.json` 索引。
 *
 * `zip_cache_get/put/del` 的 md5 → 文件映射就存在这里；进程重启后依然命中，
 * 所以第二次启动不再联网（除非 `__info.json` 里的 md5 变了）。
 */
class RemoteCache {
 public:
  struct Entry {
    std::u16string md5;
    std::u16string file;
    std::u16string version;
    long long size = 0;
    double time = 0.0;
  };

  explicit RemoteCache(std::filesystem::path dir, bool persist = true);

  const std::filesystem::path& dir() const { return _dir; }
  bool persist() const { return _persist; }

  /** 读索引（不存在 = 空缓存，不算错误） */
  bool load();
  bool save() const;

  const Entry* find(const std::u16string& md5) const;
  void put(const Entry& entry);
  void erase(const std::u16string& md5);

  std::filesystem::path file_of(const Entry& entry) const;
  std::filesystem::path zip_path(const std::u16string& md5) const;
  std::filesystem::path part_path(const std::u16string& md5) const;

  /** 路径是否落在这个缓存目录里（删文件前用它兜底） */
  bool owns(const std::filesystem::path& path) const;

  std::size_t count() const { return _entries.size(); }

 private:
  std::filesystem::path _index_path() const;

  std::filesystem::path _dir;
  bool _persist = true;
  std::map<std::u16string, Entry> _entries;
};

}  // namespace lfw::host
