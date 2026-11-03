#pragma once

#include <string>

namespace lfw::host::net {

inline constexpr int kNetUps = 60;
inline constexpr double kLockstepRttRatio = 1.25;
inline constexpr double kRttJitterMargin = 1.3;
inline constexpr int kMinLead = 2;
inline constexpr int kMaxLead = 8;
inline constexpr int kUnknownRttMs = 100;

enum class SyncMode { kLockstep, kDelay };

enum class SyncChoice { kAuto, kLockstep, kDelay };

struct SyncPlan {
  SyncMode mode = SyncMode::kLockstep;
  int input_delay = 1;
};

const char* sync_mode_name(SyncMode mode);
bool parse_sync_mode(const std::u16string& text, SyncMode& out);
bool parse_sync_choice(const std::u16string& text, SyncChoice& out);

int delay_lead(double max_rtt_ms, int ups = kNetUps);
SyncPlan recommend_sync(double max_rtt_ms, int ups = kNetUps);
SyncPlan resolve_sync(SyncChoice choice, double max_rtt_ms, int ups = kNetUps);

}
