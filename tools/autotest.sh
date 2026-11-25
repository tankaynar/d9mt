#!/bin/zsh
# Reproducible GTA SA test run on d9mt.
#   tools/autotest.sh <scenario> [--trace] [--fps] [--shot N] [--exit N] [--timeout S] [VAR=value ...]
# Scenarios live in tools/autotest/scenarios/<name>.ini (read by d9mt_autotest.asi, installed in
# modloader/zz d9mt autotest/). Frame numbers count presents after the mod marks the scene ready.
# Results: bench/auto/<scenario>-<YYYYmmdd-HHMMSS>/ (shot.png, fps.txt, autotest.log, x87run.log, trace.txt).
# Extra VAR=value arguments are exported to the game (e.g. D9MT_NO_GAMMA=1, D9MT_BB_X8=0).
set -u
REPO="${0:A:h:h}"
SCEN="${1:?usage: autotest.sh <scenario> [--trace] [--fps] [--shot N] [--exit N] [--timeout S] [VAR=value ...]}"; shift
SCEN_FILE="$REPO/tools/autotest/scenarios/$SCEN.ini"
[[ -f "$SCEN_FILE" ]] || { echo "no scenario $SCEN_FILE"; exit 2; }
SHOT=60 EXIT=90 FPS_FROM=0 TRACE=0 TIMEOUT=240
while (( $# )); do
  case "$1" in
    --trace) TRACE=1 ;;
    --fps) FPS_FROM=30; EXIT=1830 ;;            # ~30 s of frames after settling
    --shot) SHOT="$2"; shift ;;
    --exit) EXIT="$2"; shift ;;
    --timeout) TIMEOUT="$2"; shift ;;
    *=*) export "$1" ;;
    *) echo "unknown argument $1"; exit 2 ;;
  esac
  shift
done

WRAP="$HOME/Applications/Sikarugir/GtaSA.app/Contents"
GAME="$WRAP/SharedSupport/prefix/drive_c/Program Files/GTA_SA_1.0"
LOGS="$WRAP/SharedSupport/Logs"
LAUNCHER="$HOME/Applications/Sikarugir/GTA SA (fast x87 + Metal).command"
kill_game() {
  DYLD_FALLBACK_LIBRARY_PATH="$WRAP/Frameworks" WINEPREFIX="$WRAP/SharedSupport/prefix" WINEMSYNC=1 WINEESYNC=1 \
    "$WRAP/SharedSupport/wine/bin/wineserver" -k 2>/dev/null
}

# Wine only starts rendering/processing once its window is activated (normally by a click). Activate the game's
# macOS process programmatically (NSRunningApplication; no Accessibility permission needed).
activate_game() {
  local pids=($(pgrep -f 'gta_sa.exe'))
  (( ${#pids} )) || return
  osascript -l JavaScript "$REPO/tools/autotest/activate.js" $pids >/dev/null 2>&1  # best effort; the mod forces ForegroundApp
}

if pgrep -q -f 'gta_sa.exe'; then echo "game is running — quit it first"; exit 3; fi
[[ -f "$GAME/modloader/zz d9mt autotest/d9mt_autotest.asi" ]] || { echo "d9mt_autotest.asi not installed"; exit 3; }

OUT="$REPO/bench/auto/$SCEN-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
rm -rf "$GAME/d9mt_auto"
cp "$SCEN_FILE" "$GAME/d9mt_autotest_scenario.ini"

export D9MT_AUTOTEST='d9mt_autotest_scenario.ini' D9MT_AUTO_WAIT_MARK=1 D9MT_NO_FOCUS_HOOK=1
export D9MT_AUTO_SHOT_AT="$SHOT" D9MT_AUTO_EXIT_AT="$EXIT"
(( FPS_FROM )) && export D9MT_AUTO_FPS_FROM="$FPS_FROM"
if (( TRACE )); then export D9MT_TRACE_RT=2 D9MT_AUTO_TRACE_AT="$SHOT"; fi

echo "autotest: $SCEN  shot@$SHOT exit@$EXIT fps_from=$FPS_FROM trace=$TRACE  -> $OUT"
zsh "$LAUNCHER" >/dev/null 2>&1 &
start=$SECONDS
while (( SECONDS - start < TIMEOUT )); do
  [[ -f "$GAME/d9mt_auto/done.txt" ]] && break
  activate_game
  sleep 2
done
result=ok
[[ -f "$GAME/d9mt_auto/done.txt" ]] || result=timeout
sleep 3
kill_game
sleep 2
pgrep -q -f 'gta_sa.exe' && { kill_game; sleep 2; }

cp -R "$GAME/d9mt_auto/." "$OUT/" 2>/dev/null
cp "$LOGS/x87run.log" "$OUT/x87run.log" 2>/dev/null
grep -a "d9mt RT trace" "$OUT/x87run.log" > "$OUT/trace.txt" 2>/dev/null
[[ -s "$OUT/trace.txt" ]] || rm -f "$OUT/trace.txt"
[[ -f "$OUT/shot.bmp" ]] && sips -s format png "$OUT/shot.bmp" --out "$OUT/shot.png" >/dev/null 2>&1 && rm -f "$OUT/shot.bmp"
rm -f "$GAME/d9mt_autotest_scenario.ini"

echo "result: $result ($(( SECONDS - start ))s)"
[[ -f "$OUT/fps.txt" ]] && echo "fps: $(cat "$OUT/fps.txt")"
[[ -f "$OUT/autotest.log" ]] && tail -4 "$OUT/autotest.log"
grep -a -m1 "Unhandled page fault" "$OUT/x87run.log" 2>/dev/null
echo "$OUT"
