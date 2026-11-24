#pragma once
// Diagnostics. D9MT_FAIL logs a failing HRESULT (first 8 times per call site);
// D9MT_TRACE counts calls per method; d9mt_dump_call_counts() logs and resets them.
#include "log/log.hpp"
#include "util_string.hpp"
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#define D9MT_FAIL(hr)                                                                              \
  ([&]() {                                                                                         \
    static int d9mt_fail_count_ = 0;                                                               \
    if (d9mt_fail_count_++ < 8)                                                                    \
      ::dxmt::Logger::warn(::dxmt::str::format("D3D9 FAIL ", #hr, " (", __FILE__, ":", __LINE__, ")")); \
    return (hr);                                                                                   \
  }())
namespace d9mt_trace {
struct Slot { const char *name; std::atomic<unsigned> count; };
inline Slot g_slots[256];
inline std::atomic<unsigned> g_used{0};
inline Slot *slot_for(const char *name) {
  unsigned n = g_used.load();
  for (unsigned i = 0; i < n; i++) if (g_slots[i].name == name) return &g_slots[i];
  unsigned i = g_used.fetch_add(1);
  if (i >= 256) return nullptr;
  g_slots[i].name = name; return &g_slots[i];
}

// D9MT_HEAPWATCH=1: tripwire for the ucrtbase heap. The 8-byte block headers of the
// heap's free-list sentinels (heap+0x98+16*i) never change legitimately; we snapshot
// them and check on every D3D9 call and from a polling thread, logging who was active.
namespace heapwatch {
constexpr int kEntries = 16;
inline std::atomic<int> g_state{0}; // 0 = uninit, 1 = on, 2 = off
inline unsigned char *g_heap = nullptr;
inline const char *g_ring[16];
inline std::atomic<unsigned> g_seq{0};
inline std::atomic<bool> g_reported[kEntries];
inline uint32_t rd(uintptr_t a) { return *(volatile uint32_t *)a; }
// free-list sentinel i: list (next,prev) at heap+0x98+16*i. Consistent when next->prev == self && prev->next == self.
inline bool bad(int i, uint32_t &n, uint32_t &p) {
  uintptr_t L = (uintptr_t)g_heap + 0x98 + 16 * i;
  n = rd(L); p = rd(L + 4);
  if (IsBadReadPtr((void *)(uintptr_t)n, 8) || IsBadReadPtr((void *)(uintptr_t)p, 8)) return true;
  return rd(n + 4) != L || rd(p) != L;
}
inline void report(int i, uint32_t n, uint32_t p, const char *who) {
  if (g_reported[i].exchange(true)) return;
  unsigned seq = g_seq.load();
  std::string out = ::dxmt::str::format("HEAPWATCH: freelist ", i, " at ", (void *)(g_heap + 0x98 + 16 * i),
      " inconsistent next=", (void *)(uintptr_t)n, " prev=", (void *)(uintptr_t)p,
      " detected by ", who, " tid=", GetCurrentThreadId(), " callseq=", seq, " last calls:");
  for (unsigned k = 0; k < 16 && k < seq; k++) out += ::dxmt::str::format(" ", g_ring[(seq - 1 - k) & 15]);
  ::dxmt::Logger::err(out);
  fprintf(stderr, "%s\n", out.c_str()); fflush(stderr);
}
inline void check(const char *who) {
  for (int i = 0; i < kEntries; i++) {
    uint32_t n, p;
    if (!bad(i, n, p)) continue;
    SwitchToThread(); Sleep(1); // ignore transient states while another thread is mid-update
    if (bad(i, n, p)) report(i, n, p, who);
  }
}
inline DWORD WINAPI poller(void *) {
  for (;;) { check("poller"); YieldProcessor(); SwitchToThread(); }
  return 0;
}
inline void init() {
  int expected = 0;
  if (!g_state.compare_exchange_strong(expected, 2)) return;
  const char *e = getenv("D9MT_HEAPWATCH");
  if (!e || !*e || *e == '0') return;
  auto fn = (intptr_t(__cdecl *)(void))GetProcAddress(GetModuleHandleA("ucrtbase.dll"), "_get_heap_handle");
  if (!fn) { ::dxmt::Logger::err("HEAPWATCH: no _get_heap_handle"); return; }
  g_heap = (unsigned char *)fn();
  std::string out = ::dxmt::str::format("HEAPWATCH: ucrt heap ", (void *)g_heap, " headers:");
  for (int i = 0; i < kEntries; i++) {
    uint32_t n, p; bool b = bad(i, n, p);
    out += ::dxmt::str::format(" ", i, b ? "!" : "=", (void *)(uintptr_t)n);
  }
  ::dxmt::Logger::info(out);
  g_state = 1;
  CreateThread(nullptr, 0, poller, nullptr, 0, nullptr);
}
inline void on_call(const char *name) {
  if (g_state.load(std::memory_order_relaxed) == 0) init();
  if (g_state.load(std::memory_order_relaxed) != 1) return;
  check(name);
  unsigned s = g_seq.fetch_add(1);
  g_ring[s & 15] = name;
}
}
inline std::atomic<unsigned> g_enc_batches{0}, g_enc_draws{0}, g_enc_clear_color{0}, g_enc_clear_ds{0}, g_enc_present{0};
inline void dump_and_reset() {
  ::dxmt::Logger::info(::dxmt::str::format("encoded: batches=", g_enc_batches.exchange(0), " draws=", g_enc_draws.exchange(0),
      " clearColor=", g_enc_clear_color.exchange(0), " clearDS=", g_enc_clear_ds.exchange(0), " presents=", g_enc_present.exchange(0)));
  std::string out = "calls:";
  unsigned n = g_used.load(); if (n > 256) n = 256;
  for (unsigned i = 0; i < n; i++) {
    unsigned c = g_slots[i].count.exchange(0);
    if (c) out += ::dxmt::str::format(" ", g_slots[i].name, "=", c);
  }
  ::dxmt::Logger::info(out);
}
}
#define D9MT_TRACE() \
  do { static ::d9mt_trace::Slot *d9mt_slot_ = ::d9mt_trace::slot_for(__func__); if (d9mt_slot_) d9mt_slot_->count++; ::d9mt_trace::heapwatch::on_call(__func__); } while (0)
