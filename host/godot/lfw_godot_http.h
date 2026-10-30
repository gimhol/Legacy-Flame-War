#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lfw::host {

/**
 * 同步 HTTP GET（内部用 `godot::HTTPClient` 自旋 poll）。
 *
 * `ILfwHost` 的取数接口都是同步的，而 Godot 的 HTTP 是异步的 ⇒ 只能在加载期阻塞自旋。
 * 只用于启动/进关的加载阶段，别在 `_process` 里调。
 */

struct HttpUrl {
  bool ok = false;
  bool tls = false;
  std::string host;
  int port = 80;
  /** path + query，至少是 "/" */
  std::string target;
};

HttpUrl parse_http_url(const std::u16string& url);

bool is_http_url(const std::u16string& url);

/** 收到一段响应体时回调；返回 false 表示中止 */
using HttpChunkFn = std::function<bool(const uint8_t* data, size_t size)>;

/** 进度回调：已收字节 / 总字节（总长未知时 total < 0） */
using HttpProgressFn = std::function<void(long long received, long long total)>;

struct HttpResult {
  int status = 0;
  long long total = -1;
  /** 206：服务端按 Range 续上了 */
  bool partial = false;
  long long received = 0;
};

/** 取回整个响应体 */
bool http_get(const std::u16string& url, const std::vector<std::string>& headers,
              std::vector<uint8_t>& out, HttpResult& result, std::u16string& error,
              const HttpProgressFn& progress = {});

/**
 * 下载到文件（`append` = 断点续传，从现有大小发 `Range: bytes=<size>-`）。
 * 服务端不支持 Range（回 200）时会清空重下。只写文件，不负责 md5 校验。
 */
bool http_download(const std::u16string& url, const std::filesystem::path& dest, bool append,
                   std::vector<std::string> headers, HttpResult& result, std::u16string& error,
                   const HttpProgressFn& progress = {});

}  // namespace lfw::host
