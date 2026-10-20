# Peer inspiration (ideas only — no source code)

Public review of [SolidCraft](https://github.com/tangency-labs/solidcraft) (tangency-labs; MIT OR Apache-2.0; pure Rust; truck kernel).  
**Rule:** same as Autodesk — behaviour and docs as reference. **Do not copy SolidCraft (or truck) source into Fission.** Their licence is permissive, but mixing it into a GPLv3 Qt/OCCT tree without a deliberate dual-licence plan is still a mess; more importantly we stay clean-room.

## Positioning reminder

| | Fission 360 | SolidCraft (public) |
|---|---|---|
| Kernel | OpenCASCADE 7.6 | truck (Rust) |
| Licence | GPL-3.0-or-later | MIT OR Apache-2.0 |
| UI | Qt Quick | egui + wgpu (+ wasm web) |
| Strong suit | STEP/IGES depth, Fusion import, slicer handoff | Multi-platform packaging, MCP/CLI, community scale |

Lead with **interop + print**, not “another Fusion clone.”

## Ideas worth adopting (re-implement, do not copy)

Prioritised for Fission’s existing C++/Qt/OCCT stack.

### High leverage (do next)

1. **Command registry / one surface for UI + scripts + agents**  
   SolidCraft: everything is an engine command; toolbar, CLI, scripts and MCP share it ([architecture](https://docs.solidcraft.io/dev/architecture.html), [mcp.md](https://github.com/tangency-labs/solidcraft/blob/main/docs/mcp.md)).  
   Fission already has ScriptHost / JS commands — push toward a single named command table that QML, scripts, and a future MCP all call.

2. **MCP (Model Context Protocol) server**  
   Headless or bridged to the running app (`--connect`). Huge for “AI helps model” without rewriting the kernel.  
   Implement as a thin process over Fission’s command registry + document API.

3. **Headless CLI: measure / snapshot / run script**  
   `solidcraft-cli run`, `snapshot`, `eval` for STEP.  
   Fission: extend `--selftest` / ScriptHost into `Fission360 --run script.json --export out.step` and PNG snapshot (we already have demo screenshot flags).

4. **Oracle / reference-part tests**  
   They check volume, area, CoM, bbox against analytic or FreeCAD oracles ([capabilities](https://github.com/tangency-labs/solidcraft/blob/main/docs/capabilities.md)).  
   Fission: grow `--selftest` with a small set of STEP/`.f360` golden parts and numeric tolerances (we already have volume checks).

5. **Release packaging maturity**  
   MSI / winget, DMG, AppImage, Snap, unsigned-download UX notes.  
   Fission: finish Windows installer + winget; keep pursuing Mac/Linux per `docs/ci-release-sketch.md`.

6. **Recorded demo GIF + scripted demo**  
   Their README opens with a rebuild demo.  
   Fission: keep `docs/screenshot.png`; add a short demo GIF from `--demo` / UI test when packaging allows.

### Medium leverage

7. **Create Issue from Help** with context (feature error, optional screenshot) — easy Qt/QDesktopServices + GitHub issue URL.  
8. **3D mouse** — HID/SpaceMouse if demand appears.  
9. **Current-view DXF** (what you see, hidden edges dashed) — we have drawing DXF; viewport DXF is a nice inspect export.  
10. **Configurations** (parameter rows / suppress features) — natural extension of our parameters panel.  
11. **JSON command scripts** as first-class files (list of `{command, args}`) — aligns with ScriptHost.  
12. **Clear “early but usable” + oracle score** in the README so expectations match SolidCraft’s honesty.

### Low leverage / different stack (don’t chase blindly)

- Wasm / browser CAD — huge project; not Fission’s near-term edge.  
- Replacing OCCT with truck — throws away Fusion/STEP strengths.  
- Their tessellation planner / compute shaders — interesting later for viewport speed; not a copy-paste.

## What we should *not* do

- Vendor or paste SolidCraft / truck / egui code into this repo.  
- Clone their UI pixel-for-pixel or QML that mirrors Autodesk (IP rule unchanged).  
- Compete on star count; compete on **Fusion bridge + slicer + OCCT fidelity**.

## Suggested Fission roadmap chips (from this review)

- [x] Formal `CommandRegistry` (string id → invoke) — QML `fissionCommands`, ScriptHost fall-through, MCP *(1.13.29)*  
- [x] `Fission360 --mcp` MVP (list_commands, execute, batch, measure, export, open/save, …) — see `docs/mcp.md` *(1.13.29; screenshot / --connect later)*  
- [ ] Oracle suite: 10 public STEP parts, volume/CoM vs FreeCAD or analytic  
- [ ] Windows MSI + `winget` manifest  
- [ ] Help → Report Issue (pre-filled body: version, OS, last error)  
- [ ] README “honest status” line + link to this file  

Reviewed against SolidCraft public README / capabilities / architecture / MCP docs (2026-10). Re-check their CONTRIBUTING/AGENTS before any contribution *to* SolidCraft (separate project).
