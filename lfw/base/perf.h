#pragma once

#include <string>

#ifndef LFW_PERF
#define LFW_PERF 0
#endif

namespace lfw {

enum PerfSlot {
  kPerfStep,
  kPerfRender,
  kPerfAssign,
  kPerfSyncText,
  kPerfDraw,
  kPerfWorldPre,
  kPerfWorldBuffs,
  kPerfWorldEntities,
  kPerfWorldSort,
  kPerfWorldBroad,
  kPerfBroadLookup,
  kPerfBroadGet,
  kPerfBroadAdd,
  kPerfColBound,
  kPerfColCommit,
  kPerfColPrep,
  kPerfColTest,
  kPerfWorldHandlers,
  kPerfWorldGones,
  kPerfWorldStage,
  kPerfEntTimers,
  kPerfEntDataset,
  kPerfEntSyncCtrl,
  kPerfEntFusion,
  kPerfEntRecoverHp,
  kPerfEntVrests,
  kPerfEntOpoint,
  kPerfEntRecover,
  kPerfEntFramePick,
  kPerfEntPhysics,
  kPerfEntCtrl,
  kPerfEntTail,
  kPerfRenderBuild,
  kPerfRenderUi,
  kPerfRenderOrder,
  kPerfRenderEmit,
  kPerfEntRecoverMp,
  kPerfEntHpData,
  kPerfEntHpSet,
  kPerfEntBlink,
  kPerfEntCtrlUpdate,
  kPerfEntCatch,
  kPerfColPrecheck,
  kPerfEntHpFrame,
  kPerfSetHpSummary,
  kPerfSetHpCb,
  kPerfSetHpDead,
  kPerfFrameTotal,
  kPerfSlotCount
};

enum PerfCounter {
  kPerfProcesses,
  kPerfSteps,
  kPerfRenders,
  kPerfDraws,
  kPerfDrawItems,
  kPerfDrawOps,
  kPerfTexGets,
  kPerfTintGets,
  kPerfSilGets,
  kPerfPairs,
  kPerfCollisionGets,
  kPerfCollisions,
  kPerfColCalls,
  kPerfColEmpty,
  kPerfColWork,
  kPerfColCubeHits,
  kPerfColCommits,
  kPerfLookups,
  kPerfLookupIters,
  kPerfLookupVisits,
  kPerfCounterCount
};

#if LFW_PERF

namespace perf {

using NowFn = double (*)();

inline NowFn& now_fn() {
  static NowFn fn = nullptr;
  return fn;
}

inline void set_now(NowFn fn) { now_fn() = fn; }

inline double now_ms() {
  const NowFn fn = now_fn();
  return fn != nullptr ? fn() : 0.0;
}

inline const char* const* slot_names() {
  static const char* const names[kPerfSlotCount] = {
      "step",         "render",       "assign",       "sync_text",    "draw",
      "world_pre",    "world_buff",   "world_ent",    "world_sort",   "world_broad",
      "broad_lookup", "broad_get",    "broad_add",    "col_bound",    "col_commit_t",
      "col_prep",     "col_test",
      "world_handle", "world_gone",   "world_stage",  "ent_timers",   "ent_dataset",
      "ent_sync",     "ent_fusion",   "ent_recover_hp", "ent_vrests", "ent_opoint",
      "ent_recover",  "ent_frame",    "ent_physics",  "ent_ctrl",     "ent_tail",
      "render_build", "render_ui",    "render_order", "render_emit", "ent_recover_mp",
      "ent_hp_data",  "ent_hp_set",   "ent_blink",    "ent_ctrl_upd", "ent_catch",
      "col_precheck", "ent_hp_frame", "set_hp_summary", "set_hp_cb",  "set_hp_dead",
      "frame_total"};
  return names;
}

inline const char* const* counter_names() {
  static const char* const names[kPerfCounterCount] = {"processes", "steps",  "renders",
                                                       "draws",     "items",  "draw_ops",
                                                       "tex_gets",  "tints",  "silhouettes",
                                                       "pairs",     "col_gets", "col_hits",
                                                       "col_calls", "col_empty", "col_work",
                                                       "col_cube",  "col_commit", "lookups",
                                                       "walk_iters", "walk_visits"};
  return names;
}

inline std::string whole_str(long long v) {
  if (v == 0) return "0";
  const bool neg = v < 0;
  unsigned long long u =
      neg ? 0ULL - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
  char buf[24];
  int i = 0;
  while (u > 0) {
    buf[i++] = static_cast<char>('0' + static_cast<int>(u % 10ULL));
    u /= 10ULL;
  }
  std::string out = neg ? "-" : "";
  while (i > 0) out += buf[--i];
  return out;
}

inline std::string fixed2(double v) {
  const bool neg = v < 0.0;
  const double a = neg ? -v : v;
  const long long scaled = static_cast<long long>(a * 100.0 + 0.5);
  std::string out = neg ? "-" : "";
  out += whole_str(scaled / 100);
  out += '.';
  const long long frac = scaled % 100;
  if (frac < 10) out += '0';
  out += whole_str(frac);
  return out;
}

struct Accumulator {
  double ms[kPerfSlotCount] = {};
  long long counts[kPerfCounterCount] = {};
  double first_ms = 0;

  void begin_process() {
    if (first_ms == 0.0) first_ms = now_ms();
    ++counts[kPerfProcesses];
  }
};

inline Accumulator& acc() {
  static Accumulator a;
  return a;
}

inline void reset() {
  Accumulator& a = acc();
  for (int i = 0; i < kPerfSlotCount; ++i) a.ms[i] = 0.0;
  for (int i = 0; i < kPerfCounterCount; ++i) a.counts[i] = 0;
  a.first_ms = 0.0;
}

class ScopedTimer {
 public:
  explicit ScopedTimer(double* slot) : _slot(slot), _t0(now_ms()) {}
  ~ScopedTimer() { *_slot += now_ms() - _t0; }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  double* _slot;
  double _t0;
};

class Lap {
 public:
  explicit Lap(PerfSlot slot) : _slot(&acc().ms[slot]), _t0(now_ms()) {}
  ~Lap() { stop(); }
  void stop() {
    if (_slot == nullptr) return;
    *_slot += now_ms() - _t0;
    _slot = nullptr;
  }
  Lap(const Lap&) = delete;
  Lap& operator=(const Lap&) = delete;

 private:
  double* _slot;
  double _t0;
};

inline std::string summary(long long frames, double render_cost_ms) {
  const Accumulator& a = acc();
  const double wall = a.first_ms > 0.0 ? now_ms() - a.first_ms : 0.0;
  const double n = frames > 0 ? static_cast<double>(frames) : 1.0;

  double accounted = 0;
  std::string out = "[perf] per-frame ms:";
  for (int i = 0; i < kPerfSlotCount; ++i) {
    if (a.ms[i] <= 0.0) continue;
    if (i <= kPerfDraw) accounted += a.ms[i];
    out += " ";
    out += slot_names()[i];
    out += "=";
    out += fixed2(a.ms[i] / n);
  }
  out += " other=";
  out += fixed2((wall - accounted) / n);
  out += " total=";
  out += fixed2(wall / n);

  out += "\n[perf] counts:";
  for (int i = 0; i < kPerfCounterCount; ++i) {
    if (a.counts[i] == 0) continue;
    out += " ";
    out += counter_names()[i];
    out += "=";
    out += whole_str(a.counts[i]);
  }
  if (a.counts[kPerfDraws] > 0) {
    out += " | per-draw: items=";
    out += whole_str(a.counts[kPerfDrawItems] / a.counts[kPerfDraws]);
    out += " ops=";
    out += whole_str(a.counts[kPerfDrawOps] / a.counts[kPerfDraws]);
  }

  out += "\n[perf] wall=";
  out += fixed2(wall);
  out += "ms frames=";
  out += whole_str(frames);
  out += " render_cost=";
  out += fixed2(render_cost_ms);
  out += "ms";
  if (a.ms[kPerfFrameTotal] > 0.0) {
    out += " frame=";
    out += fixed2(a.ms[kPerfFrameTotal] / n);
    out += "ms engine=";
    out += fixed2((wall - a.ms[kPerfFrameTotal]) / n);
    out += "ms";
  }
  return out;
}

}

#define LFW_PERF_CAT2(a, b) a##b
#define LFW_PERF_CAT(a, b) LFW_PERF_CAT2(a, b)

#define LFW_PERF_SCOPE(slot) \
  ::lfw::perf::ScopedTimer LFW_PERF_CAT(lfw_perf_scope_, __LINE__)(&::lfw::perf::acc().ms[slot])
#define LFW_PERF_LAP(name, slot) ::lfw::perf::Lap name(slot)
#define LFW_PERF_COUNT(counter) (++::lfw::perf::acc().counts[counter])
#define LFW_PERF_ADD(counter, n) (::lfw::perf::acc().counts[counter] += (n))
#define LFW_PERF_ADD_MS(slot, v) (::lfw::perf::acc().ms[slot] += (v))
#define LFW_PERF_STAMP(name) const double name = ::lfw::perf::now_ms()
#define LFW_PERF_BEGIN_PROCESS() (::lfw::perf::acc().begin_process())
#define LFW_PERF_RESET() (::lfw::perf::reset())
#define LFW_PERF_NOW_MS() (::lfw::perf::now_ms())

#else

namespace perf {

class Lap {
 public:
  explicit Lap(PerfSlot) {}
  void stop() {}
  Lap(const Lap&) = delete;
  Lap& operator=(const Lap&) = delete;
};

}

#define LFW_PERF_SCOPE(slot) ((void)0)
#define LFW_PERF_LAP(name, slot) ::lfw::perf::Lap name(slot)
#define LFW_PERF_COUNT(counter) ((void)0)
#define LFW_PERF_ADD(counter, n) ((void)0)
#define LFW_PERF_ADD_MS(slot, v) ((void)0)
#define LFW_PERF_STAMP(name) ((void)0)
#define LFW_PERF_BEGIN_PROCESS() ((void)0)
#define LFW_PERF_RESET() ((void)0)
#define LFW_PERF_NOW_MS() (0.0)

#endif

}
