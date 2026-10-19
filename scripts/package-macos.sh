#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Sketch: package Fission360.app for macOS. See docs/ci-release-sketch.md
# Usage (after a Release build that produces a .app):
#   bash scripts/package-macos.sh [build-dir]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VER="$(sed -n 's/.*project(Fission360 VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
ARCH="$(uname -m)"
case "$ARCH" in
  arm64) ARCH_TAG=arm64 ;;
  x86_64) ARCH_TAG=x64 ;;
  *) ARCH_TAG="$ARCH" ;;
esac
TAG="Fission360-${VER}-macos-${ARCH_TAG}"
BUILD="${1:-build}"
OUT="dist/${TAG}"
ZIP="dist/${TAG}.zip"

APP=""
for cand in "$BUILD/Fission360.app" "$BUILD/Release/Fission360.app" "$BUILD/Fission360"; do
  if [[ -d "$cand" && "$cand" == *.app ]]; then APP="$cand"; break; fi
  if [[ -x "$cand" && ! -d "$cand" ]]; then
    echo "error: expected a MACOSX_BUNDLE .app under $BUILD (found bare binary $cand)." >&2
    echo "Wire MACOSX_BUNDLE + macdeployqt in CMake before packaging." >&2
    exit 1
  fi
done
if [[ -z "$APP" ]]; then
  echo "error: no Fission360.app under $BUILD — sketch only until CMake produces a bundle." >&2
  exit 1
fi

# Prefer macdeployqt if not already run
if command -v macdeployqt >/dev/null 2>&1; then
  macdeployqt "$APP" -qmldir="$ROOT/assets/qml" || true
fi

rm -rf "$OUT" "$ZIP"
mkdir -p "$OUT"
cp -R "$APP" "$OUT/"
cp LICENSE THIRD_PARTY_NOTICES.md README.md "$OUT/"
cat > "$OUT/README-RELEASE.txt" <<EOF
Fission 360 ${VER} (macOS ${ARCH_TAG})

Open Fission360.app. Unsigned builds need right-click ▸ Open the first time (Gatekeeper).

License: LICENSE (GPL-3.0-or-later)
Third-party: THIRD_PARTY_NOTICES.md
EOF

ditto -c -k --sequesterRsrc --keepParent "$OUT" "$ZIP"
echo "Packaged: $ZIP"
