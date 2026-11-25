#include "wsi_platform.hpp"
#include <windows.h>
#include <cstdlib>
#include <cstring>

namespace dxmt::wsi {

// d9mt: large blocks (argument/staging buffers, big vertex buffers) come straight from VirtualAlloc:
// 64 KB aligned and sized in whole 16 KB host pages, instead of the CRT heap (blocks start at odd 4 KB
// offsets inside heap regions). Under Rosetta, a 16 KB host page whose 4 KB sub-pages differ in
// protection is trapped on every access; heap-allocated argbuf blocks produced one such page that
// took ~80k write faults/s. D9MT_VALLOC=0 restores the heap path.
static constexpr size_t kVirtualAllocThreshold = 64 * 1024;
static constexpr size_t kHostPage = 16 * 1024;

static bool
use_valloc() {
  static const bool on = [] {
    const char *e = getenv("D9MT_VALLOC");
    return !(e && *e == '0');
  }();
  return on;
}

void *
aligned_malloc(size_t size, size_t alignment) {
  void *ptr;
  if (use_valloc() && size >= kVirtualAllocThreshold && alignment <= 64 * 1024) {
    size_t rounded = (size + kHostPage - 1) & ~(kHostPage - 1);
    ptr = VirtualAlloc(nullptr, rounded, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    size = rounded;
  } else {
    ptr = _aligned_malloc(size, alignment);
  }
  // Pre-fault every page now, before Metal wraps the memory.
  if (ptr)
    std::memset(ptr, 0, size);
  return ptr;
}

void
aligned_free(void *ptr) {
  if (!ptr)
    return;
  // VirtualAlloc'd blocks are region bases; _aligned_malloc pointers never are (heap header precedes them).
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(ptr, &mbi, sizeof(mbi)) && mbi.AllocationBase == ptr && mbi.Type == MEM_PRIVATE) {
    VirtualFree(ptr, 0, MEM_RELEASE);
    return;
  }
  _aligned_free(ptr);
}

} // namespace dxmt::wsi
