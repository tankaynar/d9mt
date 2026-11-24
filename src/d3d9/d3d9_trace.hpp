#pragma once
// Diagnostics. D9MT_FAIL logs a failing HRESULT (first 8 times per call site);
// D9MT_TRACE logs method entry only when built with -DD9MT_TRACE_CALLS.
#include "log/log.hpp"
#include "util_string.hpp"
#define D9MT_FAIL(hr)                                                                              \
  ([&]() {                                                                                         \
    static int d9mt_fail_count_ = 0;                                                               \
    if (d9mt_fail_count_++ < 8)                                                                    \
      ::dxmt::Logger::warn(::dxmt::str::format("D3D9 FAIL ", #hr, " in ", __func__, " (", __FILE__, \
                                               ":", __LINE__, ")"));                               \
    return (hr);                                                                                   \
  }())
#ifdef D9MT_TRACE_CALLS
#define D9MT_TRACE() \
  do { static int d9mt_calls_ = 0; if (d9mt_calls_++ < 100000) ::dxmt::Logger::info(::dxmt::str::format("D3D9 call ", __func__)); } while (0)
#else
#define D9MT_TRACE() do {} while (0)
#endif
