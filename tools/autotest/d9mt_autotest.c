// d9mt_autotest.asi — reproducible GTA SA 1.0 US test scenarios for d9mt (see tools/autotest.sh).
//
// Inert unless the environment variable D9MT_AUTOTEST names a scenario file (Windows path). Then it:
//   1. skips the intro movies, 2. loads save slot <slot> straight from the main menu (replays the menu's own
//   "load game" action), 3. once CJ exists: forces weather/time every frame, optionally teleports him and sets his
//   heading, puts the camera behind him, 4. after <settle> frames calls d3d9.dll's d9mt_autotest_mark() so d9mt's
//   D9MT_AUTO_* screenshot/trace/exit frames are counted from a stable scene.
// Log: d9mt_auto\autotest.log in the game folder.
//
// All addresses are GTA SA 1.0 US and were verified by disassembling gta_sa.exe (2026-11-25):
//   Idle 0x53E920 calls CTimer::Update 0x561B10 at 0x53E968; FrontendIdle 0x53E770 calls it at 0x53E777.
//   gGameState 0xC8D4C0 (7 = frontend, 9 = playing). FrontEndMenuManager 0xBA6748: +0x15F selected slot,
//   +0xE9 skip-settings flag, +0x32, +0x5D start-loading, +0x60 loading-saved-game (main loop 0x748DE5/0x748DFC).
//   CheckSlotDataValid 0x5D1380 (cdecl char(int)), DoSettingsBeforeStartingAGame 0x573330 (thiscall),
//   menu load action 0x578EEA. FindPlayerPed 0x56E210, CPed::Teleport 0x5E4110 (thiscall CVector,bool),
//   ped heading +0x558/+0x55C, CWeather::ForceWeatherNow 0x72A4F0, clock 0xB70153 h / 0xB70152 m / 0xB70150 s,
//   TheCamera 0xB6F028, CCamera::SetCameraDirectlyBehindForFollowPed 0x50BD40 (thiscall).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

#define GAME_STATE (*(volatile int *)0xC8D4C0)
#define MENU ((volatile uint8_t *)0xBA6748)

typedef void(__cdecl *Void_t)(void);
typedef void *(__cdecl *FindPlayerPed_t)(int);
typedef char(__cdecl *CheckSlot_t)(int);
typedef void(__cdecl *ForceWeatherNow_t)(int);
typedef void(__attribute__((thiscall)) * ThisVoid_t)(void *);
typedef void(__attribute__((thiscall)) * Teleport_t)(void *, float, float, float, int);

static Void_t g_orig_frontend_update, g_orig_idle_update;
static char g_dir[MAX_PATH]; // game folder (gta_sa.exe's directory); modloader changes the cwd while loading ASIs
static FILE *g_log;
static struct {
  int slot, hour, minute, weather, skip_intro, settle, has_pos, has_heading, cam_behind, area;
  float x, y, z, heading_deg;
} cfg = {0, -1, -1, -1, 1, 120, 0, 0, 1, -1, 0, 0, 0, 0};

static void logf_(const char *fmt, ...) {
  if (!g_log) return;
  SYSTEMTIME t;
  GetLocalTime(&t);
  fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  va_list a;
  va_start(a, fmt);
  vfprintf(g_log, fmt, a);
  va_end(a);
  fputc('\n', g_log);
  fflush(g_log);
}

static void load_scenario(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) { logf_("cannot open scenario %s", path); return; }
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char *eq = strchr(line, '=');
    if (!eq || line[0] == '#' || line[0] == ';') continue;
    *eq = 0;
    const char *k = line, *v = eq + 1;
    if (!strcmp(k, "slot")) cfg.slot = atoi(v);
    else if (!strcmp(k, "hour")) cfg.hour = atoi(v);
    else if (!strcmp(k, "minute")) cfg.minute = atoi(v);
    else if (!strcmp(k, "weather")) cfg.weather = atoi(v);
    else if (!strcmp(k, "skip_intro")) cfg.skip_intro = atoi(v);
    else if (!strcmp(k, "settle")) cfg.settle = atoi(v);
    else if (!strcmp(k, "cam_behind")) cfg.cam_behind = atoi(v);
    else if (!strcmp(k, "pos")) cfg.has_pos = sscanf(v, "%f,%f,%f", &cfg.x, &cfg.y, &cfg.z) == 3;
    else if (!strcmp(k, "area")) cfg.area = atoi(v);
    else if (!strcmp(k, "heading")) { cfg.heading_deg = (float)atof(v); cfg.has_heading = 1; }
  }
  fclose(f);
  logf_("scenario: slot=%d time=%d:%02d weather=%d pos=%s(%.1f,%.1f,%.1f) heading=%s%.1f settle=%d",
        cfg.slot, cfg.hour, cfg.minute, cfg.weather, cfg.has_pos ? "" : "keep ", cfg.x, cfg.y, cfg.z,
        cfg.has_heading ? "" : "keep ", cfg.heading_deg, cfg.settle);
}

static void freeze_world(void) {
  if (cfg.weather >= 0) ((ForceWeatherNow_t)0x72A4F0)(cfg.weather);
  if (cfg.hour >= 0) {
    *(volatile uint8_t *)0xB70153 = (uint8_t)cfg.hour;
    *(volatile uint8_t *)0xB70152 = (uint8_t)(cfg.minute < 0 ? 0 : cfg.minute);
    *(volatile uint16_t *)0xB70150 = 0;
  }
}

static int g_phase = 0, g_frames = 0, g_last_state = -1;

static void tick(void) {
  int state = GAME_STATE;
  if (state != g_last_state) { logf_("game state %d -> %d (phase %d)", g_last_state, state, g_phase); g_last_state = state; }
  g_frames++;
  switch (g_phase) {
  case 0: // wait for the main menu, then replay the menu's load-slot action
    if (cfg.skip_intro && (state == 2 || state == 4)) { GAME_STATE = state + 1; logf_("skipped movie state %d", state); }
    if (state == 7 && g_frames > 30) {
      MENU[0x15F] = (uint8_t)cfg.slot;
      if (!((CheckSlot_t)0x5D1380)(cfg.slot)) { logf_("slot %d not valid — aborting", cfg.slot); g_phase = 9; break; }
      if (!MENU[0xE9]) ((ThisVoid_t)0x573330)((void *)MENU);
      MENU[0x32] = 1;
      *(volatile uint8_t *)0xB72910 = 0;
      MENU[0x60] = 1;
      logf_("load of slot %d requested", cfg.slot);
      g_phase = 1;
      g_frames = 0;
    }
    break;
  case 1: // wait until playing with a player ped
    if (state == 9 && ((FindPlayerPed_t)0x56E210)(-1)) { g_phase = 2; g_frames = 0; logf_("in game, player ped present"); }
    break;
  case 2: { // set up the scene (teleport once, heading/camera for a while), then settle
    void *ped = ((FindPlayerPed_t)0x56E210)(-1);
    if (!ped) break;
    freeze_world();
    if (g_frames == 30 && cfg.has_pos && cfg.area != 0 && *(volatile int *)0xB72914 != cfg.area && cfg.area > 0)
      logf_("area %d requested but only area 0 (outside) is supported", cfg.area);
    if (g_frames == 30 && cfg.has_pos && cfg.area <= 0 && *(volatile int *)0xB72914 != 0) {
      // Mirror the game's own "leave interior" sequence (0x4422C6): saves load CJ inside the safehouse, and a teleport
      // alone leaves the area code set, so the outside world is never streamed/drawn (white world, empty radar).
      logf_("leaving interior area %d", *(volatile int *)0xB72914);
      *((volatile uint8_t *)ped + 0x2F) = 0;
      *(volatile int *)((uint8_t *)ped + 0x78C) = 0;
      *(volatile int *)0x96A7C4 = 0;
      *(volatile int *)0xB72914 = 0;
      *(volatile uint8_t *)0xC0FCB6 = 0;
      ((void(__cdecl *)(int))0x4094B0)(0);
      *(volatile uint8_t *)0xC402D3 = 0;
      ((void(__cdecl *)(int))0x55FF20)(0); // CTimeCycle::StopExtraColour: interiors use the timecycle's interior
                                           // "extra colours" (white/black sky, black fog) until this is called
    }
    if (g_frames == 30 && cfg.has_pos) {
      ((Teleport_t)0x5E4110)(ped, cfg.x, cfg.y, cfg.z, 0);
      logf_("teleported to %.2f %.2f %.2f", cfg.x, cfg.y, cfg.z);
    }
    if (g_frames >= 30 && g_frames < 30 + cfg.settle) {
      if (cfg.has_heading) {
        float r = cfg.heading_deg * 3.14159265f / 180.0f;
        *(volatile float *)((uint8_t *)ped + 0x558) = r;
        *(volatile float *)((uint8_t *)ped + 0x55C) = r;
      }
      if (cfg.cam_behind) ((ThisVoid_t)0x50BD40)((void *)0xB6F028);
    }
    if (g_frames == 30 + cfg.settle) {
      HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
      Void_t mark = d3d9 ? (Void_t)GetProcAddress(d3d9, "d9mt_autotest_mark") : NULL;
      logf_("scene ready, mark %s", mark ? "sent to d9mt" : "unavailable (not d9mt)");
      if (mark) mark();
      g_phase = 3;
    }
    break;
  }
  case 3: // hold the scene still until d9mt exits the process
    freeze_world();
    break;
  default:
    break;
  }
}

// The main loop (0x748A87) sleeps while ForegroundApp (0x8D621C) is 0, i.e. until the window is activated by a
// click. Test runs happen in the background, so keep the flag set (plain store; the loop only reads it).
// Movie states 2/4 (0x748B17) wait for "movie finished" or a key, then gGameState++ (0x748BBE). DirectShow cannot play
// them under Wine here, so do the same increment once a movie state has lasted 500 ms (CAS: never double-step).
static DWORD WINAPI foreground_thread(LPVOID unused) {
  int same = 0, last = -1;
  for (;;) {
    *(volatile int *)0x8D621C = 1;
    int st = GAME_STATE;
    same = (st == last) ? same + 1 : 0;
    last = st;
    if (cfg.skip_intro && (st == 2 || st == 4) && same >= 25) {
      if (InterlockedCompareExchange((volatile LONG *)0xC8D4C0, st + 1, st) == st) logf_("skipped movie state %d", st);
      same = 0;
    }
    Sleep(20);
  }
  return 0;
}

static void __cdecl frontend_update_hook(void) { g_orig_frontend_update(); tick(); }
static void __cdecl idle_update_hook(void) { g_orig_idle_update(); tick(); }

static int hook_call(uintptr_t site, void *fn, Void_t *orig) {
  uint8_t *p = (uint8_t *)site;
  if (p[0] != 0xE8) { logf_("call site %08x is not a call (%02x) — not hooked", (unsigned)site, p[0]); return 0; }
  DWORD old;
  VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
  *orig = (Void_t)(site + 5 + *(int32_t *)(p + 1)); // chain to whatever is installed (game or another mod)
  *(int32_t *)(p + 1) = (int32_t)((uintptr_t)fn - (site + 5));
  VirtualProtect(p, 5, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, 5);
  logf_("hooked call %08x (previous target %08x)", (unsigned)site, (unsigned)(uintptr_t)*orig);
  return 1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH) return TRUE;
  const char *scen = getenv("D9MT_AUTOTEST");
  if (!scen || !*scen) return TRUE; // normal play: do nothing
  GetModuleFileNameA(NULL, g_dir, MAX_PATH);
  char *slash = strrchr(g_dir, '\\');
  if (slash) *slash = 0;
  char path[MAX_PATH * 2];
  snprintf(path, sizeof path, "%s\\d9mt_auto", g_dir);
  CreateDirectoryA(path, NULL);
  snprintf(path, sizeof path, "%s\\d9mt_auto\\autotest.log", g_dir);
  g_log = fopen(path, "w");
  logf_("d9mt_autotest loaded, game dir %s, scenario %s", g_dir, scen);
  if (scen[0] && scen[1] == ':') snprintf(path, sizeof path, "%s", scen);
  else snprintf(path, sizeof path, "%s\\%s", g_dir, scen);
  load_scenario(path);
  hook_call(0x53E777, (void *)frontend_update_hook, &g_orig_frontend_update);
  hook_call(0x53E968, (void *)idle_update_hook, &g_orig_idle_update);
  CloseHandle(CreateThread(NULL, 0, foreground_thread, NULL, 0, NULL));
  return TRUE;
}
