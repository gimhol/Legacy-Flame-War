#include "host/net/net_sync.h"

#include <algorithm>
#include <cmath>

namespace lfw::host::net {

const char* sync_mode_name(SyncMode mode) {
  return mode == SyncMode::kDelay ? "delay" : "lockstep";
}

bool parse_sync_mode(const std::u16string& text, SyncMode& out) {
  if (text == u"delay") {
    out = SyncMode::kDelay;
    return true;
  }
  if (text == u"lockstep") {
    out = SyncMode::kLockstep;
    return true;
  }
  return false;
}

bool parse_sync_choice(const std::u16string& text, SyncChoice& out) {
  if (text == u"auto") {
    out = SyncChoice::kAuto;
    return true;
  }
  if (text == u"delay") {
    out = SyncChoice::kDelay;
    return true;
  }
  if (text == u"lockstep") {
    out = SyncChoice::kLockstep;
    return true;
  }
  return false;
}

int delay_lead(double max_rtt_ms, int ups) {
  const double safe_ups = ups > 0 ? static_cast<double>(ups) : static_cast<double>(kNetUps);
  const double tick_ms = 1000.0 / safe_ups;
  const double rtt = max_rtt_ms > 0.0 ? max_rtt_ms : static_cast<double>(kUnknownRttMs);
  const int lead = static_cast<int>(std::ceil(rtt * kRttJitterMargin / tick_ms));
  return std::min(kMaxLead, std::max(kMinLead, lead));
}

SyncPlan recommend_sync(double max_rtt_ms, int ups) {
  const double safe_ups = ups > 0 ? static_cast<double>(ups) : static_cast<double>(kNetUps);
  const double tick_ms = 1000.0 / safe_ups;
  const double rtt = max_rtt_ms > 0.0 ? max_rtt_ms : static_cast<double>(kUnknownRttMs);
  if (rtt <= tick_ms * kLockstepRttRatio) return {SyncMode::kLockstep, 1};
  return {SyncMode::kDelay, delay_lead(max_rtt_ms, ups)};
}

SyncPlan resolve_sync(SyncChoice choice, double max_rtt_ms, int ups) {
  if (choice == SyncChoice::kLockstep) return {SyncMode::kLockstep, 1};
  if (choice == SyncChoice::kDelay) return {SyncMode::kDelay, delay_lead(max_rtt_ms, ups)};
  return recommend_sync(max_rtt_ms, ups);
}

}
