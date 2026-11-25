#pragma once

// d9mt diag: bytes of FRESH memory obtained per allocator category (interval counters, logged and reset
// every 600 frames). Used to find what touches ~500 MB/s of new pages outdoors in GTA SA (~127k faults/s).

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace d9mt_allocstats {

enum Category : unsigned {
  RingBlockArgbuf,   // new argument-buffer ring block
  RingBlockStaging,  // new staging ring block
  RingBlockHost,     // new host (CPU command) ring block
  RingBlockOther,
  PoolMiss,          // placed_pool miss -> aligned_malloc
  TexMip,            // d3d9 managed texture CPU mip copies
  EncCtxChunk,       // ArgumentEncodingContext CPU heap chunk
  kCount
};

inline const char *const kNames[kCount] = {"ring_argbuf", "ring_staging", "ring_host", "ring_other",
                                           "pool_miss",   "tex_mip",      "encctx_chunk"};

inline std::atomic<uint64_t> g_bytes[kCount];
inline std::atomic<uint64_t> g_count[kCount];

inline void
add(Category c, uint64_t bytes) {
  g_bytes[c].fetch_add(bytes, std::memory_order_relaxed);
  g_count[c].fetch_add(1, std::memory_order_relaxed);
}

inline void
log_and_reset() {
  char buf[512];
  int n = snprintf(buf, sizeof(buf), "d9mt allocstats (fresh MB / count since last):");
  for (unsigned i = 0; i < kCount && n > 0 && n < (int)sizeof(buf); i++) {
    uint64_t b = g_bytes[i].exchange(0), c = g_count[i].exchange(0);
    n += snprintf(buf + n, sizeof(buf) - n, " %s=%.1f/%llu", kNames[i], b / 1048576.0, (unsigned long long)c);
  }
  fprintf(stderr, "%s\n", buf);
}

} // namespace d9mt_allocstats
