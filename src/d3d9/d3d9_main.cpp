#include <cstdio>
#include <cstdlib>

#include "d3d9_interface.hpp"
#include "log/log.hpp"

namespace dxmt {
Logger Logger::s_instance("d3d9.log");

extern "C" IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion) {
  Logger::info("Direct3DCreate9 called");
  // d9mt experiment: D9MT_DEP=1 enables DEP/NX for the process. Without it Wine maps all readable memory of a
  // 32-bit process as host-executable (rwx); Rosetta then write-protects such pages and every write to one of
  // our argument-buffer pages trapped into its exception server (~80k faults/s outdoors in GTA SA).
  if (const char *e = getenv("D9MT_DEP"); e && *e == '1') {
    BOOL ok = SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
    DWORD flags = 0;
    BOOL permanent = FALSE;
    GetProcessDEPPolicy(GetCurrentProcess(), &flags, &permanent);
    fprintf(stderr, "d9mt DEP: SetProcessDEPPolicy=%d err=%lu flags=%lu permanent=%d\n", (int)ok,
            ok ? 0ul : (unsigned long)GetLastError(), (unsigned long)flags, (int)permanent);
  }
  return new D3D9Interface();
}

} // namespace dxmt

#ifndef DXMT_NATIVE

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);
  return TRUE;
}

#endif
