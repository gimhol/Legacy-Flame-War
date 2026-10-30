#include "host/godot/lfw_godot_remote_cache.h"

#include <fstream>
#include <memory>
#include <system_error>
#include <vector>

#include "lfw/core/json.h"
#include "lfw/core/json5.h"
#include "lfw/core/value.h"
#include "lfw/utils/container_help/field_or.h"
#include "lfw/utils/utf8.h"

namespace lfw::host {

namespace {

constexpr double kIndexVersion = 1.0;

std::u16string to_u16(const std::string& s) {
  return lfw::decode_utf8(std::vector<uint8_t>(s.begin(), s.end()));
}

std::string to_u8(const std::u16string& s) {
  const std::vector<uint8_t> buf = lfw::encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

}  // namespace

RemoteCache::RemoteCache(std::filesystem::path dir, bool persist)
    : _dir(std::move(dir)), _persist(persist) {}

std::filesystem::path RemoteCache::_index_path() const { return _dir / "cache.json"; }

bool RemoteCache::load() {
  _entries.clear();
  if (!_persist) return true;
  std::error_code ec;
  const std::filesystem::path index = _index_path();
  if (!std::filesystem::is_regular_file(index, ec)) return true;

  std::ifstream in(index, std::ios::binary);
  if (!in) return false;
  std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (raw.empty()) return true;

  const std::u16string text = to_u16(raw);
  Value root;
  const Json5Result parsed = json5_parse(text);
  if (parsed.ok) {
    root = parsed.value;
  } else {
    const std::optional<Value> strict = json_parse(text);
    if (!strict.has_value()) return false;
    root = *strict;
  }

  const Value entries = lfw::field_or(root, u"entries");
  const Object* const obj = as_object(entries);
  if (obj == nullptr) return true;
  for (const std::u16string& key : obj->keys()) {
    const Value* const raw_entry = obj->get(key);
    if (raw_entry == nullptr) continue;
    Entry entry;
    entry.md5 = key;
    entry.file = lfw::to_string(lfw::field_or(*raw_entry, u"file"));
    entry.version = lfw::to_string(lfw::field_or(*raw_entry, u"version"));
    if (entry.file.empty()) entry.file = key + u".zip";
    const Value size_value = lfw::field_or(*raw_entry, u"size");
    if (const double* const size = std::get_if<double>(&size_value)) {
      entry.size = static_cast<long long>(*size);
    }
    const Value time_value = lfw::field_or(*raw_entry, u"time");
    if (const double* const time = std::get_if<double>(&time_value)) {
      entry.time = *time;
    }
    _entries[key] = entry;
  }
  return true;
}

bool RemoteCache::save() const {
  if (!_persist) return true;
  std::error_code ec;
  std::filesystem::create_directories(_dir, ec);

  auto entries = std::make_shared<Object>();
  for (const auto& kv : _entries) {
    auto item = std::make_shared<Object>();
    item->set(u"file", Value(kv.second.file));
    item->set(u"version", Value(kv.second.version));
    item->set(u"size", Value(static_cast<double>(kv.second.size)));
    item->set(u"time", Value(kv.second.time));
    entries->set(kv.first, Value(item));
  }
  auto root = std::make_shared<Object>();
  root->set(u"version", Value(kIndexVersion));
  root->set(u"entries", Value(entries));

  const std::optional<std::u16string> text = json_stringify(Value(root));
  if (!text.has_value()) return false;

  std::ofstream out(_index_path(), std::ios::binary | std::ios::trunc);
  if (!out) return false;
  const std::string utf8 = to_u8(*text);
  out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
  return out.good();
}

const RemoteCache::Entry* RemoteCache::find(const std::u16string& md5) const {
  const auto it = _entries.find(md5);
  return it == _entries.end() ? nullptr : &it->second;
}

void RemoteCache::put(const Entry& entry) {
  Entry copy = entry;
  if (copy.file.empty()) copy.file = copy.md5 + u".zip";
  _entries[copy.md5] = copy;
}

void RemoteCache::erase(const std::u16string& md5) { _entries.erase(md5); }

std::filesystem::path RemoteCache::file_of(const Entry& entry) const {
  return _dir / std::filesystem::path(to_u8(entry.file));
}

std::filesystem::path RemoteCache::zip_path(const std::u16string& md5) const {
  if (const Entry* const entry = find(md5)) return file_of(*entry);
  return _dir / std::filesystem::path(to_u8(md5 + u".zip"));
}

std::filesystem::path RemoteCache::part_path(const std::u16string& md5) const {
  std::filesystem::path path = zip_path(md5);
  path += ".part";
  return path;
}

bool RemoteCache::owns(const std::filesystem::path& path) const {
  std::error_code ec;
  const std::filesystem::path a = std::filesystem::weakly_canonical(path, ec);
  if (ec) return false;
  const std::filesystem::path b = std::filesystem::weakly_canonical(_dir, ec);
  if (ec) return false;
  const auto a_it = a.begin();
  auto b_it = b.begin();
  auto a_cur = a_it;
  for (; b_it != b.end(); ++b_it) {
    if (a_cur == a.end() || *a_cur != *b_it) return false;
    ++a_cur;
  }
  return true;
}

}  // namespace lfw::host
