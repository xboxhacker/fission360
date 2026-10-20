# SolidCraft issue parity (ideas / tests only)

Public scan of [tangency-labs/solidcraft issues](https://github.com/tangency-labs/solidcraft/issues) (249 issues pulled 2026-10-20).  
**Rule:** behaviour and failure modes as reference. **Do not copy SolidCraft (or truck) source.**

Many SolidCraft bugs are kernel-specific (`not supported yet: …` from truck). Fission uses OpenCASCADE, so the same geometry often succeeds — still worth locking with `--selftest` so we do not regress into the same *user-visible* failures (bad volume, silent empty boolean, invalid STEP faces, hangs).

## Covered in `testSolidCraftParity()` (SelfTest)

| SC issue | Failure mode | Fission check |
|---|---|---|
| [#3541](https://github.com/tangency-labs/solidcraft/issues/3541) | Circle loft pinches mid-height | Ruled circle frustum volume + bbox diameter |
| [#3578](https://github.com/tangency-labs/solidcraft/issues/3578) | Combine join → no result | Overlapping boxes fuse to 1500 mm³ |
| [#3526](https://github.com/tangency-labs/solidcraft/issues/3526) | Intersect on multi-lump body silently empty | Distant join keeps 2 solids; intersect keeps one lump |
| [#3528](https://github.com/tangency-labs/solidcraft/issues/3528) | Cut near revolve seam removes too much | Revolve tube + cut keeps >70% volume |
| [#3573](https://github.com/tangency-labs/solidcraft/issues/3573) | Shell after taper+fillet → invalid faces | Shell healthy + `BRepCheck` valid + volume down |
| [#3561](https://github.com/tangency-labs/solidcraft/issues/3561) | Oversized fillet hangs / bad geom | Fails with error **or** valid solid |
| [#3587](https://github.com/tangency-labs/solidcraft/issues/3587) | Chamfer taller than walls | Same graceful fail / valid rule |
| [#3563](https://github.com/tangency-labs/solidcraft/issues/3563) | Extrude join / through-all fails | Join pad + through-all cut volumes |

Also related (already covered elsewhere in SelfTest): open-top Shell, hip-roof Shell (1.13.31), combine cut/intersect, loft square frustum, patterns, draft, hole through-all.

## Reviewed, not encoded as geometry recipes (yet)

| Theme | Examples | Why skipped / Fission note |
|---|---|---|
| Sketch solver / SVG / face attach | #3590, #3530, #3529, #3586, #3514 | SolidCraft sketch stack; different constraint engine |
| UI / shortcuts / macOS | #3589, #3580, #3579, #3568, #3521 | App chrome, not OCCT solids |
| Free-form / spline shell | [#3585](https://github.com/tangency-labs/solidcraft/issues/3585) | SC: only analytic faces; OCCT shell can do B-splines — need a user STEP before a fair test |
| Boundary / patch surface | #3570 | Surface workspace depth differs |
| Chamfer at concave / face pick | #3575, #3577, #3565 | Need exact edge topology from their reports |
| Fillet chain / hang on face | #3572, #3549, #3562, #3533 | Vague or UI hang; watch in dogfood |
| Offset spline certificate | #3574 | Sketch offset specifics |
| Nested offset plane UX | #3567 | Construction-plane editor |
| Thread face pick | #3519 | Already have modeled thread selftests |
| Split body / multi rename | #3518, #3510 | Partial coverage via SplitBody plane test |
| Crash in `document.inspect` | #3560 | Inspect selftests exist; keep an eye on large models |
| Extrude hang / freeze | #3540, #3536, #3550 | Performance; no reliable unit recipe without their files |

## How to extend

1. Pick an open `area:kernel` issue with a **public-safe** size recipe (or attach a tiny STEP you author yourself).  
2. Add a block under `testSolidCraftParity()` named `sc-parity #NNNN: …`.  
3. Prefer volume + `BRepCheck_Analyzer` + `isHealthy()` over pixel screenshots.  
4. Link the issue here in the covered table.

Re-scan SolidCraft issues periodically when preparing a release; do not vendor their repro files if they look like proprietary user IP.