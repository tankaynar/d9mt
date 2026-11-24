#include "wsi_platform.hpp"
#include <cstdlib>
#include <cstring>

namespace dxmt::wsi {

void *aligned_malloc(size_t size, size_t alignment) {
  void *ptr = _aligned_malloc(size, alignment);
  // Pre-fault every page now. This memory is usually wrapped by Metal right away
  // (newBufferWithBytesNoCopy); on Apple Silicon + Rosetta/Wine, the first CPU touch of a page
  // after Metal has mapped it can stall for seconds. Touching it first makes later writes free.
  if (ptr)
    std::memset(ptr, 0, size);
  return ptr;
}

void aligned_free(void *ptr) { return _aligned_free(ptr); }

} // namespace dxmt::wsi
