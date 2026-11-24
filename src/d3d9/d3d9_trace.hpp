#pragma once
// Diagnostic: log every failing HRESULT return with its source location.
#include "log/log.hpp"
#include "util_string.hpp"
#define D9MT_FAIL(hr) \
  (::dxmt::Logger::warn(::dxmt::str::format("D3D9 FAIL ", #hr, " in ", __func__, " (", __FILE__, ":", __LINE__, ")")), (hr))
