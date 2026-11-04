#pragma once

#include <string>
#include <vector>

namespace lfw::host::net {

/**
 * 联机传输：只负责收发 UTF-8 JSON 文本，不认识协议。
 * 第一版实现走 WebSocket（复用 Web 端已有的中继服务器），第二版换成 Steam P2P 时只换实现。
 */
class INetTransport {
 public:
  virtual ~INetTransport() = default;

  virtual void connect(const std::string& url) = 0;
  /** 推进内部状态（非阻塞）；真正的收发在 `poll` 里完成 */
  virtual void poll() = 0;
  /** 取出自上次调用以来收到的全部文本；返回是否拿到了东西 */
  virtual bool drain(std::vector<std::string>& out) = 0;
  virtual void send(const std::string& text) = 0;
  virtual bool connected() const = 0;
  /** 上一次 `connect` 是否被传输层当场拒绝（URL 非法/协议不支持），用于立刻换下一个候选 */
  virtual bool failed() const { return false; }
  /** 平滑后的 RTT（毫秒）；测不出来就返回 0 */
  virtual int rtt_ms() const { return 0; }
};

}
