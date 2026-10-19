# CI / multi-platform release sketch

**Status:** design only. Windows packages are built locally today
(`scripts/package-windows.ps1`). macOS and Linux jobs below will not produce
binaries until the CMake blockers in §2 are fixed and runners have Qt + OCCT.

Workflow stub: [`.github/workflows/release.yml`](../.github/workflows/release.yml).  
Packaging stubs: `scripts/package-macos.sh`, `scripts/package-linux.sh`.

---

## 1. Goal

On each version tag `vX.Y.Z` (and optionally `workflow_dispatch`):

| Artifact | Format | Contents |
|---|---|---|
| `Fission360-X.Y.Z-windows-x64.zip` | Zip of folder | exe + Qt/OCCT/WebEngine + LICENSE + THIRD_PARTY_NOTICES.md + README |
| `Fission360-X.Y.Z-macos-arm64.dmg` (or `.zip` of `.app`) | DMG preferred | `Fission360.app` after `macdeployqt` |
| `Fission360-X.Y.Z-linux-x64.AppImage` (or `.tar.gz`) | AppImage preferred | Relocatable tree + `linuxdeployqt` / `linuxdeploy` |

Every artifact must ship `LICENSE` + `THIRD_PARTY_NOTICES.md` and a link to the
matching source tag (GPL source offer).

Self-test gate: `Fission360 --selftest` (or `.app/Contents/MacOS/…`) must exit 0
**or** match the known loft baseline (document allowed failures in the workflow).

---

## 2. CMake work required before Mac/Linux CI can succeed

Today `CMakeLists.txt` is Windows-shaped. Before CI greenlights non-Windows:

1. **OCCT discovery** — accept a normal install, not only OrcaSlicer deps:
   - `find_package(OpenCASCADE)` / `OpenCASCADEConfig.cmake`, **or**
   - `-DOCCT_ROOT=` with `include/`, `lib/`, `bin/` (or `.dylib` / `.so`).
   - Keep `FISSION_ORCA_ROOT` as a Windows/dev convenience.
2. **Qt discovery** — already uses `CMAKE_PREFIX_PATH`; document aqt paths per OS:
   - Windows: `…/6.8.2/msvc2022_64`
   - macOS: `…/6.8.2/macos` (or `clang_64`)
   - Linux: `…/6.8.2/gcc_64`
3. **POST_BUILD deploy** — branch on OS:
   - Windows: keep `windeployqt` + OCCT DLL copy.
   - macOS: `macdeployqt $<TARGET_BUNDLE_DIR:Fission360> -qmldir=…` (need `MACOSX_BUNDLE`).
   - Linux: skip in-tree deploy; packaging script runs `linuxdeploy` / `linuxdeployqt`.
4. **MSVC-only flags** — keep `/EHa` behind `if(MSVC)`.
5. **OpenGL libs** — `opengl32 glu32` are Windows; use `${OPENGL_LIBRARIES}` / `OpenGL::GL` on Unix.
6. **WebEngine** — confirm Qt WebEngine is available for the chosen aqt target on each OS (it is the heavy / fragile piece).
7. **Optional:** `install(TARGETS …)` + CPack generators (`ZIP`, `DragNDrop`, `External` for AppImage) so packaging is less script-heavy.

Until (1)–(5) land, Mac/Linux workflow jobs should stay `continue-on-error: true` or disabled.

---

## 3. Runner & dependency strategy

```
                  ┌─────────────────┐
  tag vX.Y.Z ───► │  release.yml    │
                  └────────┬────────┘
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    windows-2022      macos-14         ubuntu-22.04
    VS2022 + aqt      aqt macOS        aqt linux
    OCCT cache        OCCT cache       OCCT cache / apt
    package-windows   package-macos    package-linux
           │               │               │
           └───────────────┴───────────────┘
                           ▼
                  GitHub Release assets
```

### Qt

- Cache `aqtinstall` output under `~/Qt/6.8.2/…` (or `D:/a/Qt/…` on Windows).
- Modules: Desktop + **WebEngine** matching the kit (`win64_msvc2022_64`, `clang_64` / macOS, `linux_gcc_64`).
- Set `CMAKE_PREFIX_PATH` to that kit.

### OCCT 7.6

Pick **one** long-term approach (recommended order):

1. **Prebuilt cache** — build OCCT once per OS/arch, store as a Release asset or GitHub Actions cache keyed by OCCT git SHA + OS. CI only downloads.
2. **vcpkg / conan** — if/when OCCT 7.6 ports match our needs (verify STEP/XCAF/mesh toolkits).
3. **Build from source on the runner** — slow (~30–90 min); only as bootstrap to fill the cache.
4. **FISSION_ORCA_ROOT** — fine for local Windows; **do not** clone all of OrcaSlicer on CI just for OCCT unless you already maintain that mirror.

### Secrets / signing (later)

- Windows: optional Authenticode (`WINDOWS_CERT_*`).
- macOS: Apple Developer ID + notarization (`MACOS_CERTIFICATE`, `APPLE_ID`, `APP_PASSWORD`, team ID). Unsigned `.app` is OK for early CI artifacts; Gatekeeper will block casual users.
- Linux: no signing required for AppImage; optional GPG detach-sig.

---

## 4. Job sketch (per OS)

Common steps:

1. Checkout (this repo only; no `docs/fusion_*`).
2. Restore Qt + OCCT caches (or install/build).
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=… -DOCCT_ROOT=…`
4. `cmake --build build --config Release --target Fission360`
5. Run `--selftest` (headless; on Linux may need `xvfb-run`).
6. Run `scripts/package-*.{ps1,sh}`.
7. Upload artifact; on tag, attach to GitHub Release.

Windows can mirror the local script almost immediately once Qt/OCCT are on the runner.  
macOS/Linux stay yellow until §2 is done.

---

## 5. Packaging script contracts

### Windows (done)

`scripts/package-windows.ps1` → `dist/Fission360-<ver>-windows-x64.zip`

### macOS (stub)

`scripts/package-macos.sh` → `dist/Fission360-<ver>-macos-<arch>.zip` of `Fission360.app`  
(later: `create-dmg` or `hdiutil` → `.dmg`)

Assumes CMake produced a bundle and `macdeployqt` already ran (or the script runs it).

### Linux (stub)

`scripts/package-linux.sh` → `dist/Fission360-<ver>-linux-x64.AppImage`  
Fallback: `.tar.gz` of a `Fission360.AppDir` if AppImage tooling is missing.

Copies LICENSE / notices into the payload either way.

---

## 6. Suggested implementation order

1. Refactor CMake OCCT + OpenGL + POST_BUILD (Unix-safe) — **unblocks everything**.
2. Prove a manual Linux build on a VM/WSL2 with system or cached OCCT.
3. Prove a manual macOS build on a Mac (or borrowed Actions minute).
4. Turn on `release.yml` Windows job with cached Qt/OCCT.
5. Enable Linux job → AppImage.
6. Enable macOS job → zip/app; add notarization last.

---

## 7. Out of scope for the first CI cut

- ARM Linux, Intel Mac (add matrix later: `macos-13` x64 vs `macos-14` arm64).
- Flatpak / Snap / Chocolatey / Homebrew casks.
- Building OrcaSlicer itself inside CI.
- Shipping Autodesk reference trees (must stay gitignored).
