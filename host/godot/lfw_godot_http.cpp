#include "host/godot/lfw_godot_http.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "lfw/utils/utf8.h"

namespace lfw::host {

namespace {

constexpr int kMaxRedirects = 5;
constexpr double kIdleTimeoutSec = 60.0;
constexpr double kTotalTimeoutSec = 900.0;

std::string to_u8(const std::u16string& s) {
  const std::vector<uint8_t> buf = lfw::encode_utf8(s);
  return std::string(buf.begin(), buf.end());
}

std::u16string from_u8(const std::string& s) {
  return lfw::decode_utf8(std::vector<uint8_t>(s.begin(), s.end()));
}

std::string trim_ascii(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
  return s.substr(b, e - b);
}

std::string lower_ascii(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string header_of(const godot::PackedStringArray& headers, const char* name) {
  const std::string want = lower_ascii(name);
  for (int64_t i = 0; i < headers.size(); ++i) {
    const std::string line = headers[i].utf8().get_data();
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    if (lower_ascii(trim_ascii(line.substr(0, colon))) != want) continue;
    return trim_ascii(line.substr(colon + 1));
  }
  return std::string();
}

void sleep_ms(int ms) {
  godot::OS* const os = godot::OS::get_singleton();
  if (os != nullptr) os->delay_usec(ms * 1000);
}

/** 响应头（状态码 + 长度 + 重定向位置） */
struct ResponseHead {
  int status = 0;
  long long content_length = -1;
  std::string location;
  std::string content_type;
};

std::u16string location_url(const HttpUrl& base, const std::string& location) {
  if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
    return from_u8(location);
  }
  const std::string scheme = base.tls ? "https://" : "http://";
  const bool default_port = (base.tls && base.port == 443) || (!base.tls && base.port == 80);
  const std::string authority =
      base.host + (default_port ? std::string() : ":" + std::to_string(base.port));
  if (!location.empty() && location[0] == '/') return from_u8(scheme + authority + location);
  const size_t slash = base.target.rfind('/');
  const std::string dir = slash == std::string::npos ? "/" : base.target.substr(0, slash + 1);
  return from_u8(scheme + authority + dir + location);
}

bool is_redirect(const ResponseHead& head) {
  return head.status >= 300 && head.status < 400 && !head.location.empty();
}

/**
 * 一次 GET。`on_head` 在响应头到手、收 body 之前调用一次（返回 false 中止）；
 * 用来在写文件前决定「续写还是清空」。`sink` 收 body 分片（返回 false 中止）。
 */
bool get_once(const HttpUrl& url, const std::vector<std::string>& headers, double timeout_sec,
              const std::function<bool(const ResponseHead&)>& on_head, const HttpChunkFn& sink,
              ResponseHead& head, long long& received, std::u16string& error) {
  godot::Ref<godot::HTTPClient> client;
  client.instantiate();
  if (!client.is_valid()) {
    error = u"[http] cannot create HTTPClient";
    return false;
  }

  godot::Ref<godot::TLSOptions> tls;
  if (url.tls) tls = godot::TLSOptions::client();
  if (client->connect_to_host(godot::String(url.host.c_str()), url.port, tls) != godot::OK) {
    error = u"[http] connect_to_host failed: " + from_u8(url.host);
    return false;
  }

  const auto started = std::chrono::steady_clock::now();
  const auto elapsed = [&started]() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  };

  while (client->get_status() == godot::HTTPClient::STATUS_RESOLVING ||
         client->get_status() == godot::HTTPClient::STATUS_CONNECTING) {
    client->poll();
    sleep_ms(1);
    if (elapsed() > timeout_sec) {
      client->close();
      error = u"[http] connect timeout: " + from_u8(url.host);
      return false;
    }
  }
  if (client->get_status() != godot::HTTPClient::STATUS_CONNECTED) {
    const int st = static_cast<int>(client->get_status());
    client->close();
    error = u"[http] connect failed (status " + from_u8(std::to_string(st)) +
            u"): " + from_u8(url.host);
    return false;
  }

  godot::PackedStringArray req;
  for (const std::string& h : headers) req.push_back(godot::String(h.c_str()));
  if (client->request(godot::HTTPClient::METHOD_GET, godot::String(url.target.c_str()), req) !=
      godot::OK) {
    client->close();
    error = u"[http] request failed: " + from_u8(url.target);
    return false;
  }

  bool head_delivered = false;
  double idle = 0.0;
  double last = elapsed();
  while (true) {
    const godot::HTTPClient::Status st = client->get_status();
    if (st == godot::HTTPClient::STATUS_BODY || st == godot::HTTPClient::STATUS_CONNECTED) {
      if (!head_delivered && client->has_response()) {
        head_delivered = true;
        head.status = client->get_response_code();
        const godot::PackedStringArray resp = client->get_response_headers();
        head.location = header_of(resp, "location");
        head.content_type = header_of(resp, "content-type");
        const std::string len = header_of(resp, "content-length");
        head.content_length = len.empty() ? -1 : std::atoll(len.c_str());
        if (on_head != nullptr && !on_head(head)) {
          client->close();
          error = u"[http] aborted by head handler";
          return false;
        }
      }
      godot::PackedByteArray chunk = client->read_response_body_chunk();
      if (chunk.size() > 0) {
        received += chunk.size();
        if (sink != nullptr && !sink(chunk.ptr(), static_cast<size_t>(chunk.size()))) {
          client->close();
          error = u"[http] aborted by sink";
          return false;
        }
        idle = 0.0;
        last = elapsed();
        continue;
      }
      if (st == godot::HTTPClient::STATUS_CONNECTED && head_delivered) break;
      client->poll();
      sleep_ms(1);
    } else if (st == godot::HTTPClient::STATUS_REQUESTING ||
               st == godot::HTTPClient::STATUS_RESOLVING ||
               st == godot::HTTPClient::STATUS_CONNECTING) {
      client->poll();
      sleep_ms(1);
    } else {
      const int code = static_cast<int>(st);
      client->close();
      error = u"[http] transfer failed (status " + from_u8(std::to_string(code)) +
              u"): " + from_u8(url.target);
      return false;
    }

    const double now = elapsed();
    idle += now - last;
    last = now;
    if (idle > kIdleTimeoutSec) {
      client->close();
      error = u"[http] read timeout: " + from_u8(url.target);
      return false;
    }
    if (now > timeout_sec) {
      client->close();
      error = u"[http] timeout: " + from_u8(url.target);
      return false;
    }
  }

  client->close();
  if (!head_delivered) {
    error = u"[http] no response: " + from_u8(url.target);
    return false;
  }
  return true;
}

}  // namespace

bool is_http_url(const std::u16string& url) {
  return url.rfind(u"http://", 0) == 0 || url.rfind(u"https://", 0) == 0;
}

HttpUrl parse_http_url(const std::u16string& url) {
  HttpUrl out;
  const std::string text = to_u8(url);
  const bool tls = text.rfind("https://", 0) == 0;
  if (!tls && text.rfind("http://", 0) != 0) return out;
  const size_t at = tls ? 8 : 7;
  const size_t slash = text.find('/', at);
  const std::string authority =
      text.substr(at, slash == std::string::npos ? std::string::npos : slash - at);
  out.target = slash == std::string::npos ? "/" : text.substr(slash);
  const size_t hash = out.target.find('#');
  if (hash != std::string::npos) out.target.resize(hash);
  const size_t colon = authority.rfind(':');
  if (colon != std::string::npos) {
    out.host = authority.substr(0, colon);
    out.port = std::atoi(authority.c_str() + colon + 1);
  } else {
    out.host = authority;
    out.port = tls ? 443 : 80;
  }
  if (out.host.empty()) return out;
  if (out.port <= 0) out.port = tls ? 443 : 80;
  out.tls = tls;
  out.ok = true;
  return out;
}

bool http_get(const std::u16string& url, const std::vector<std::string>& headers,
              std::vector<uint8_t>& out, HttpResult& result, std::u16string& error,
              const HttpProgressFn& progress) {
  std::u16string current = url;
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    const HttpUrl parsed = parse_http_url(current);
    if (!parsed.ok) {
      error = u"[http] bad url: " + current;
      return false;
    }
    out.clear();
    long long received = 0;
    ResponseHead head;
    const HttpChunkFn sink = [&](const uint8_t* data, size_t size) {
      out.insert(out.end(), data, data + size);
      if (progress != nullptr) progress(received, head.content_length);
      return true;
    };
    if (!get_once(parsed, headers, kTotalTimeoutSec, nullptr, sink, head, received, error)) {
      return false;
    }
    if (is_redirect(head)) {
      current = location_url(parsed, head.location);
      continue;
    }
    result.status = head.status;
    result.total = head.content_length;
    result.partial = head.status == 206;
    result.received = received;
    if (head.status != 200 && head.status != 206) {
      error = u"[http] status " + from_u8(std::to_string(head.status)) + u": " + current;
      return false;
    }
    return true;
  }
  error = u"[http] too many redirects: " + url;
  return false;
}

bool http_download(const std::u16string& url, const std::filesystem::path& dest, bool append,
                   std::vector<std::string> headers, HttpResult& result, std::u16string& error,
                   const HttpProgressFn& progress) {
  long long have = 0;
  if (append) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(dest, ec);
    if (!ec) have = static_cast<long long>(size);
  }
  std::vector<std::string> send = headers;
  if (have > 0) send.push_back("Range: bytes=" + std::to_string(have) + "-");

  std::u16string current = url;
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    const HttpUrl parsed = parse_http_url(current);
    if (!parsed.ok) {
      error = u"[http] bad url: " + current;
      return false;
    }

    std::ofstream file;
    long long base = 0;
    long long received = 0;
    ResponseHead head;

    const auto on_head = [&](const ResponseHead& h) {
      if (is_redirect(h)) return true;  // 不写文件，交给外层重定向
      if (h.status != 200 && h.status != 206) return true;  // 交给外层报错
      // 206 = 服务端按 Range 续上了，接着写；200 = 整包，必须清空重来
      base = (h.status == 206 && have > 0) ? have : 0;
      file.open(dest, std::ios::binary | (base > 0 ? std::ios::app : std::ios::trunc));
      return file.good();
    };
    const HttpChunkFn sink = [&](const uint8_t* data, size_t size) {
      if (!file.is_open()) return true;
      file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
      if (progress != nullptr) {
        const long long total = head.content_length > 0 ? base + head.content_length : -1;
        progress(base + received, total);
      }
      return file.good();
    };

    if (!get_once(parsed, send, kTotalTimeoutSec, on_head, sink, head, received, error)) {
      if (file.is_open()) file.close();
      return false;
    }
    if (file.is_open()) file.close();

    if (is_redirect(head)) {
      current = location_url(parsed, head.location);
      continue;
    }
    if (head.status != 200 && head.status != 206) {
      error = u"[http] status " + from_u8(std::to_string(head.status)) + u": " + current;
      return false;
    }
    result.status = head.status;
    result.partial = head.status == 206;
    result.received = base + received;
    result.total = head.content_length > 0 ? base + head.content_length : -1;
    return true;
  }
  error = u"[http] too many redirects: " + url;
  return false;
}

}  // namespace lfw::host
