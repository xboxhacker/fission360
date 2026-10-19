#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Sketch: package Linux x64 AppImage or tar.gz. See docs/ci-release-sketch.md
# Usage (after a Release build):
#   bash scripts/package-linux.sh [build-dir]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VER="$(sed -n 's/.*project(Fission360 VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
TAG="Fission360-${VER}-linux-x64"
BUILD="${1:-build}"
BIN=""
for cand in "$BUILD/Fission360" "$BUILD/Release/Fission360"; do
  [[ -x "$cand" ]] && BIN="$cand" && break
done
if [[ -z "$BIN" ]]; then
  echo "error: no Fission360 binary under $BUILD — build Release first (and finish Unix CMake)." >&2
  exit 1
fi

APPDIR="dist/${TAG}.AppDir"
rm -rf "$APPDIR" "dist/${TAG}.AppImage" "dist/${TAG}.tar.gz"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/metainfo" "$APPDIR/usr/share/doc/fission360"

cp "$BIN" "$APPDIR/usr/bin/Fission360"
# TODO: copy OCCT .so and run linuxdeploy / linuxdeployqt to pull Qt + QML + WebEngine.
cp LICENSE THIRD_PARTY_NOTICES.md README.md "$APPDIR/usr/share/doc/fission360/"

cat > "$APPDIR/AppRun" <<'EOF'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
exec "${HERE}/usr/bin/Fission360" "$@"
EOF
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/fission360.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Fission 360
Exec=Fission360
Icon=fission360
Categories=Graphics;Engineering;
EOF
# Placeholder icon name — copy assets/icons/fission360.png when packaging for real.
if [[ -f assets/icons/fission360.png ]]; then
  mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
  cp assets/icons/fission360.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/fission360.png"
  cp assets/icons/fission360.png "$APPDIR/fission360.png"
fi

# Prefer AppImage if linuxdeploy is on PATH; else tar.gz of AppDir
if command -v linuxdeploy >/dev/null 2>&1; then
  linuxdeploy --appdir "$APPDIR" --output appimage
  mv Fission360*.AppImage "dist/${TAG}.AppImage" 2>/dev/null || \
    mv ./*.AppImage "dist/${TAG}.AppImage"
  echo "Packaged: dist/${TAG}.AppImage"
else
  echo "note: linuxdeploy not found — writing tar.gz fallback (not a full relocatable Qt tree yet)."
  tar -C dist -czf "dist/${TAG}.tar.gz" "$(basename "$APPDIR")"
  echo "Packaged: dist/${TAG}.tar.gz"
fi
