#pragma once

#include <map>
#include <optional>

#include "host/net/net_protocol.h"
#include "host/net/net_sync.h"

namespace lfw::host::net {

/**
 * 每帧推进门控：只回答「下一帧能不能跑」「本地该提交第几帧」，
 * 既不认识传输也不认识世界，便于两种同步模式共用同一套驱动代码。
 */
class IFrameDriver {
 public:
  virtual ~IFrameDriver() = default;

  /** 提前提交的帧数：lockstep = 1，delay = K */
  virtual int lead() const = 0;

  virtual void start(int first_seq);
  virtual void push(FrameInputs inputs);

  virtual bool can_step() const;
  std::optional<FrameInputs> take();

  int next_seq() const { return _seq; }
  /**
   * 本地该提交的帧号：执行完第 N 帧后就该把第 N+`lead` 帧的输入交出去
   * （`lead=1` 即「每帧提前一帧提交」，与 Web 端的时序一致）。
   */
  int submit_seq() const { return _seq + lead() - 1; }
  int buffered() const { return static_cast<int>(_pending.size()); }
  int dropped() const { return _dropped; }

 protected:
  int _seq = 0;
  int _dropped = 0;
  std::map<int, FrameInputs> _pending;
};

class LockstepDriver final : public IFrameDriver {
 public:
  int lead() const override { return 1; }
};

class DelayDriver final : public IFrameDriver {
 public:
  explicit DelayDriver(int lead);
  int lead() const override { return _lead; }

 private:
  int _lead = kMinLead;
};

}
