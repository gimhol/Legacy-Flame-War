#include "host/net/net_selftest.h"

#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "host/net/net_frame_driver.h"
#include "host/net/net_protocol.h"
#include "host/net/net_sync.h"
#include "host/net/net_transport_loopback.h"

namespace lfw::host::net {

namespace {

constexpr double kTickMs = 1000.0 / static_cast<double>(kNetUps);

struct PeerRun {
  int executed = 0;
  int stalled_steps = 0;
  int dropped = 0;
  std::vector<std::string> transcript;
};

std::string cmd_for(const std::string& who, int seq) {
  return "KEY_EVENT --p=" + who + "#0 --s=1 --c=a --n=a --from=" + who + "@" + std::to_string(seq);
}

/** 两个客户端 + 一个确定性中继（=服务端）在回环网络上的整帧交换 */
bool simulate(int peer_count, double rtt_ms, int lead, int frames, std::vector<PeerRun>& out,
              std::string& error) {
  LoopbackHub hub(rtt_ms);
  std::vector<std::unique_ptr<LoopbackTransport>> transports;
  std::vector<std::unique_ptr<IFrameDriver>> drivers;
  std::vector<PeerRun> runs;
  for (int i = 0; i < peer_count; ++i) {
    auto transport = std::make_unique<LoopbackTransport>(hub, hub.attach());
    transport->connect("loopback://selftest");
    transports.push_back(std::move(transport));
    if (lead <= 1) {
      drivers.push_back(std::make_unique<LockstepDriver>());
    } else {
      drivers.push_back(std::make_unique<DelayDriver>(lead));
    }
    runs.emplace_back();
  }

  auto submit = [&](int index, const std::string& who, int seq) {
    Msg msg;
    msg.is_req = true;
    msg.pid = std::to_string(seq);
    msg.type = MsgType::kTick;
    msg.payload = make_tick_req(seq, {cmd_for(who, seq)}, std::to_string(seq % 60 == 0 ? seq : 0));
    transports[static_cast<std::size_t>(index)]->send(encode_envelope(msg));
  };

  std::vector<std::string> names;
  for (int i = 0; i < peer_count; ++i) names.push_back(std::string(1, static_cast<char>('A' + i)));

  for (int i = 0; i < peer_count; ++i) {
    drivers[static_cast<std::size_t>(i)]->start(0);
    for (int seq = 0; seq < drivers[static_cast<std::size_t>(i)]->lead(); ++seq) {
      submit(i, names[static_cast<std::size_t>(i)], seq);
    }
  }

  std::map<int, std::map<int, ClientTick>> pending;  // seq -> (peer -> 输入)
  const int max_steps = (frames + 8) * static_cast<int>(rtt_ms + kTickMs) + 4000;
  int steps = 0;
  bool done = false;
  while (!done && steps < max_steps) {
    ++steps;
    hub.advance(1.0);

    for (const std::pair<int, std::string>& item : hub.drain_server()) {
      const std::optional<Msg> msg = decode_envelope(item.second);
      if (!msg.has_value() || msg->type != MsgType::kTick) continue;
      const std::optional<ClientTick> req = parse_tick_req(*msg);
      if (!req.has_value()) continue;
      ClientTick filled = *req;
      const int from = item.first < 0 ? 0 : item.first;
      filled.client_id = names[static_cast<std::size_t>(from)];
      pending[filled.seq][from] = std::move(filled);
    }

    for (auto& kv : pending) {
      if (static_cast<int>(kv.second.size()) < peer_count) continue;
      std::vector<ClientTick> reqs;
      reqs.reserve(kv.second.size());
      for (const auto& one : kv.second) reqs.push_back(one.second);
      Msg resp;
      resp.is_req = false;
      resp.type = MsgType::kTick;
      resp.payload = make_tick_resp(kv.first, reqs);
      const std::string text = encode_envelope(resp);
      for (int i = 0; i < peer_count; ++i) hub.send_to(i, text);
    }
    for (auto it = pending.begin(); it != pending.end();) {
      it = static_cast<int>(it->second.size()) >= peer_count ? pending.erase(it) : std::next(it);
    }

    done = true;
    for (int i = 0; i < peer_count; ++i) {
      LoopbackTransport& transport = *transports[static_cast<std::size_t>(i)];
      IFrameDriver& driver = *drivers[static_cast<std::size_t>(i)];
      PeerRun& run = runs[static_cast<std::size_t>(i)];

      std::vector<std::string> inbox;
      transport.drain(inbox);
      for (const std::string& text : inbox) {
        const std::optional<Msg> msg = decode_envelope(text);
        if (!msg.has_value() || msg->type != MsgType::kTick) continue;
        const std::optional<FrameInputs> frame = parse_tick_resp(*msg);
        if (frame.has_value()) driver.push(*frame);
      }
      while (driver.can_step() && run.executed < frames) {
        const std::optional<FrameInputs> frame = driver.take();
        if (!frame.has_value()) break;
        for (const ClientTick& req : frame->reqs) {
          for (const std::string& cmd : req.cmds) run.transcript.push_back(cmd);
        }
        ++run.executed;
        submit(i, names[static_cast<std::size_t>(i)], driver.submit_seq());
      }
      if (!driver.can_step()) ++run.stalled_steps;
      run.dropped = driver.dropped();
      if (run.executed < frames) done = false;
    }
  }

  if (!done) {
    error = "在预算内没有跑完 " + std::to_string(frames) + " 帧（steps=" + std::to_string(steps) + "）";
    return false;
  }
  for (int i = 0; i < peer_count; ++i) {
    const PeerRun& run = runs[static_cast<std::size_t>(i)];
    if (run.executed != frames) {
      error = "客户端 " + std::to_string(i) + " 只执行了 " + std::to_string(run.executed) + " 帧";
      return false;
    }
    if (run.dropped != 0) {
      error = "客户端 " + std::to_string(i) + " 丢弃了 " + std::to_string(run.dropped) + " 帧";
      return false;
    }
    if (run.transcript != runs[0].transcript) {
      error = "客户端 " + std::to_string(i) + " 的命令流与客户端 0 不一致（确定性被破坏）";
      return false;
    }
  }
  out = std::move(runs);
  return true;
}

bool check_plan(std::string& error) {
  struct Case {
    double rtt;
    SyncMode mode;
    int lead;
  };
  const Case cases[] = {
      // rtt=0 表示"还没测出来"：与 TS 的 `max_rtt || UNKNOWN_RTT` 一致，按 100ms 估
      {0.0, SyncMode::kDelay, 8},
      {16.0, SyncMode::kLockstep, 1},
      {20.0, SyncMode::kLockstep, 1},
      {30.0, SyncMode::kDelay, 3},
      {80.0, SyncMode::kDelay, 7},
      {100.0, SyncMode::kDelay, 8},
      {400.0, SyncMode::kDelay, 8},
  };
  for (const Case& c : cases) {
    const SyncPlan plan = recommend_sync(c.rtt);
    if (plan.mode != c.mode || plan.input_delay != c.lead) {
      char buf[160];
      std::snprintf(buf, sizeof(buf), "rtt=%.0f 期望 %s/%d，实得 %s/%d", c.rtt,
                    sync_mode_name(c.mode), c.lead, sync_mode_name(plan.mode), plan.input_delay);
      error = buf;
      return false;
    }
  }
  if (resolve_sync(SyncChoice::kLockstep, 400.0).mode != SyncMode::kLockstep) {
    error = "resolve_sync(lockstep) 被 RTT 覆盖";
    return false;
  }
  if (resolve_sync(SyncChoice::kDelay, 0.0).mode != SyncMode::kDelay) {
    error = "resolve_sync(delay) 被 RTT 覆盖";
    return false;
  }
  return true;
}

bool check_protocol(std::string& error) {
  Msg req;
  req.is_req = true;
  req.pid = "7";
  req.type = MsgType::kTick;
  req.payload = make_tick_req(42, {"KEY_EVENT --p=A#0 --s=1 --c=a --n=a --from=A@42", "CMD2"}, "crc");
  const std::string wire = encode_envelope(req);
  const std::optional<Msg> back = decode_envelope(wire);
  if (!back.has_value()) {
    error = "Tick 请求编码后解不回来: " + wire;
    return false;
  }
  if (!back->is_req || back->pid != "7" || back->type != MsgType::kTick) {
    error = std::string("Tick 请求信封字段丢失: is_req=") + (back->is_req ? "1" : "0") + " pid='" +
            back->pid + "' type=" + msg_type_name(back->type) + " wire=" + wire;
    return false;
  }
  const std::optional<ClientTick> parsed = parse_tick_req(*back);
  if (!parsed.has_value() || parsed->seq != 42 || parsed->cmds.size() != 2 ||
      parsed->cmds[0] != "KEY_EVENT --p=A#0 --s=1 --c=a --n=a --from=A@42" || parsed->cmds[1] != "CMD2" ||
      parsed->checksum != "crc") {
    error = "Tick 请求 payload 字段丢失";
    return false;
  }
  std::vector<ClientTick> reqs(2);
  reqs[0].client_id = "B";
  reqs[0].seq = 42;
  reqs[0].cmds = {"b1"};
  reqs[1].client_id = "A";
  reqs[1].seq = 42;
  reqs[1].cmds = {"a1", "a2"};
  Msg resp;
  resp.is_req = false;
  resp.type = MsgType::kTick;
  resp.payload = make_tick_resp(42, reqs);
  const std::optional<Msg> resp_back = decode_envelope(encode_envelope(resp));
  if (!resp_back.has_value()) {
    error = "Tick 响应编码后解不回来";
    return false;
  }
  const std::optional<FrameInputs> frame = parse_tick_resp(*resp_back);
  if (!frame.has_value() || frame->seq != 42 || frame->reqs.size() != 2 ||
      frame->reqs[0].client_id != "B" || frame->reqs[0].cmds.size() != 1 ||
      frame->reqs[1].client_id != "A" || frame->reqs[1].cmds.size() != 2 ||
      frame->reqs[1].cmds[1] != "a2") {
    error = "Tick 响应的 reqs 顺序/内容被破坏";
    return false;
  }
  const SyncPlan plan{SyncMode::kDelay, 5};
  const std::optional<RoomStartInfo> started = parse_room_start(Msg{false, "", MsgType::kRoomStart,
                                                                  make_room_start_resp(1234.0, plan)});
  if (!started.has_value() || std::abs(started->seed - 1234.0) > 1e-9 ||
      started->plan.mode != SyncMode::kDelay || started->plan.input_delay != 5) {
    error = "RoomStart 的 seed/sync_mode/input_delay 往返不一致";
    return false;
  }
  return true;
}

}  // namespace

SelftestReport run_selftest() {
  SelftestReport report;
  std::string text;
  std::string error;

  if (!check_plan(error)) {
    report.text = "[net] selftest FAIL: 同步策略 " + error;
    return report;
  }
  text += "[net] selftest: sync plan ok (lockstep<=20ms, delay 2..8)\n";

  if (!check_protocol(error)) {
    report.text = "[net] selftest FAIL: 协议 " + error;
    return report;
  }
  text += "[net] selftest: protocol roundtrip ok (Tick req/resp, RoomStart)\n";

  std::vector<PeerRun> lockstep_runs;
  if (!simulate(2, 4.0, 1, 120, lockstep_runs, error)) {
    report.text = "[net] selftest FAIL: lockstep " + error;
    return report;
  }
  {
    char buf[220];
    std::snprintf(buf, sizeof(buf),
                  "[net] selftest: lockstep rtt=4ms frames=120 -> executed=%d stalls=%d drops=%d identical=yes\n",
                  lockstep_runs[0].executed, lockstep_runs[0].stalled_steps, lockstep_runs[0].dropped);
    text += buf;
  }

  std::vector<PeerRun> delay_runs;
  const SyncPlan plan = recommend_sync(80.0);
  if (!simulate(2, 80.0, plan.input_delay, 300, delay_runs, error)) {
    report.text = "[net] selftest FAIL: delay " + error;
    return report;
  }
  {
    char buf[260];
    std::snprintf(buf, sizeof(buf),
                  "[net] selftest: delay(lead=%d) rtt=80ms frames=300 -> executed=%d stalls=%d drops=%d "
                  "identical=yes\n",
                  plan.input_delay, delay_runs[0].executed, delay_runs[0].stalled_steps,
                  delay_runs[0].dropped);
    text += buf;
  }

  text += "[net] selftest PASS";
  report.ok = true;
  report.text = text;
  return report;
}

}
