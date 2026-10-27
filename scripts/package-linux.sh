#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Package Linux x64 AppDir + tar.gz (AppImage if linuxdeploy is present).
# Usage (after a Release build on Linux):
#   CMAKE_PREFIX_PATH=/opt/Qt/6.8.2/gcc_64 bash scripts/package-linux.sh build
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
  echo "error: no Fission360 binary under $BUILD — build Release first." >&2
  exit 1
fi

QT_PREFIX="${CMAKE_PREFIX_PATH:-${QT_DIR:-}}"
if [[ -z "$QT_PREFIX" || ! -d "$QT_PREFIX/lib" ]]; then
  for cand in /opt/Qt/6.8.2/gcc_64 "$HOME/Qt/6.8.2/gcc_64"; do
    if [[ -d "$cand/lib" ]]; then QT_PREFIX="$cand"; break; fi
  done
fi

APPDIR="dist/${TAG}.AppDir"
rm -rf "$APPDIR" "dist/${TAG}.AppImage" "dist/${TAG}.tar.gz"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/doc/fission360" \
         "$APPDIR/usr/plugins" "$APPDIR/usr/qml"

cp "$BIN" "$APPDIR/usr/bin/Fission360"
cp LICENSE THIRD_PARTY_NOTICES.md README.md "$APPDIR/usr/share/doc/fission360/"

# Pull non-system shared libs the binary needs (Qt + OCCT + friends)
copy_deps() {
  local bin="$1"
  ldd "$bin" | awk '/=>/ {print $3} /^\// {print $1}' | while read -r so; do
    [[ -z "$so" || ! -f "$so" ]] && continue
    case "$so" in
      /lib/*|/lib64/*|/usr/lib/*) continue ;;  # keep using the host for libc/GL/X11/OCCT
    esac
    cp -n "$so" "$APPDIR/usr/lib/" 2>/dev/null || true
  done
}
copy_deps "$BIN"

if [[ -n "$QT_PREFIX" && -d "$QT_PREFIX/plugins" ]]; then
  rsync -a --delete \
    "$QT_PREFIX/plugins/platforms" \
    "$QT_PREFIX/plugins/imageformats" \
    "$QT_PREFIX/plugins/iconengines" \
    "$QT_PREFIX/plugins/xcbglintegrations" \
    "$APPDIR/usr/plugins/" 2>/dev/null || true
  # QML modules used by the app
  for m in QtQuick QtQml Qt Qt5Compat QtWebEngine; do
    [[ -d "$QT_PREFIX/qml/$m" ]] && rsync -a "$QT_PREFIX/qml/$m" "$APPDIR/usr/qml/"
  done
  # Remaining Qt libs referenced by plugins
  find "$APPDIR/usr/plugins" "$APPDIR/usr/qml" -name '*.so*' -type f 2>/dev/null | while read -r so; do
    copy_deps "$so"
  done
fi

cat > "$APPDIR/AppRun" <<'EOF'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QML2_IMPORT_PATH="${HERE}/usr/qml"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"
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
if [[ -f assets/icons/fission360.png ]]; then
  mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
  cp assets/icons/fission360.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/fission360.png"
  cp assets/icons/fission360.png "$APPDIR/fission360.png"
fi

cat > "$APPDIR/usr/share/doc/fission360/LINUX.txt" <<EOF
Fission 360 $VER (Linux x64)

Needs system OpenCASCADE 7.8 packages (libocct-*) and a normal OpenGL/X11 stack.
Qt 6.8 libraries are bundled under usr/lib when packaged from an aqt kit.

Run: ./AppRun
Self-test: ./AppRun --selftest   (use xvfb-run if headless)
EOF

mkdir -p dist
if command -v linuxdeploy >/dev/null 2>&1; then
  linuxdeploy --appdir "$APPDIR" --output appimage
  mv Fission360*.AppImage "dist/${TAG}.AppImage" 2>/dev/null || mv ./*.AppImage "dist/${TAG}.AppImage"
  echo "Packaged: dist/${TAG}.AppImage"
else
  tar -C dist -czf "dist/${TAG}.tar.gz" "$(basename "$APPDIR")"
  echo "Packaged: dist/${TAG}.tar.gz ($(du -h "dist/${TAG}.tar.gz" | awk '{print $1}'))"
fi
