#!/bin/zsh
# Build the 32-bit d3d9.dll, mark it as a Wine builtin, and install it into the
# GtaSA wrapper's d9mt renderer folder (the CI build is kept as d3d9.dll.ci-release).
set -e
cd "$(dirname "$0")"
ninja -C build32 src/d3d9/d3d9.dll src/winemetal/winemetal.dll
OUT="$(mktemp -d)/d3d9.dll"
cp build32/src/d3d9/d3d9.dll "$OUT"
i686-w64-mingw32-strip "$OUT"
toolchains/wine/bin/winebuild --builtin "$OUT"
DEST="$HOME/Applications/Sikarugir/GtaSA.app/Contents/SharedSupport/renderer/d9mt/wine/i386-windows"
[[ -e "$DEST/d3d9.dll.ci-release" ]] || cp -p "$DEST/d3d9.dll" "$DEST/d3d9.dll.ci-release"
cp "$OUT" "$DEST/d3d9.dll"
# winemetal.dll (Windows side of the Metal bridge)
WM="$(mktemp -d)/winemetal.dll"
cp build32/src/winemetal/winemetal.dll "$WM"
i686-w64-mingw32-strip "$WM"
toolchains/wine/bin/winebuild --builtin "$WM"
[[ -e "$DEST/winemetal.dll.ci-release" ]] || cp -p "$DEST/winemetal.dll" "$DEST/winemetal.dll.ci-release"
cp "$WM" "$DEST/winemetal.dll"
echo "installed $(git rev-parse --short HEAD)$(git diff --quiet || echo '+local') -> $DEST/d3d9.dll"
