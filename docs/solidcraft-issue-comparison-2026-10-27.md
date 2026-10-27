# SolidCraft issue comparison — 2026-10-27

Fresh public scan of [tangency-labs/solidcraft](https://github.com/tangency-labs/solidcraft/issues) against Fission’s logged parity docs and current source (**1.13.45**).

**Rule:** behaviour and failure modes only. **Do not copy SolidCraft or truck source.**

Prior snapshots: `solidcraft-issue-census.md` + `solidcraft-parity-modeling.md` (2026-10-21, **2542** issues through ~#4616), and the smaller recipe list in `solidcraft-issue-parity.md`.

## Tracker delta

| | 2026-10-21 | 2026-10-27 | Δ |
|---|---:|---:|---:|
| Issues (open+closed, PRs excluded) | 2542 | **4631** | +2089 |
| Open | 1755 | **3088** | +1333 |
| Closed | 787 | **1543** | +756 |
| Highest issue # seen | ~4616 | **7812** | |

About **2106** issues were created after 2026-10-21. GitHub search returns at most 1000 hits per query; a keyword sample of **728** new open modeling-relevant issues was classified for this pass. Of the 1000 newest searchable issues, **~324** are generic `OPERATION: operation failed` / crash-template noise.

Keyword search totals today (title+body, open+closed): extrude 1228, fillet 638, combine/boolean/union/intersect 928, hole 595, sketch 1969, shell 380, loft 167, sweep 149, “operation failed” 1343, crash/freeze/hang 516.

Fission docs already mention **~2553** issue numbers (census + parity tables + selftest). Almost all issues numbered **>4616** are **not** in those docs yet.

## Already logged (still the backlog that matters)

The 2026-10-21 modeling pass judged **990** kernel-ish titles. Counts then:

| Verdict | Count |
|---|---:|
| ALREADY-HANDLED | 22 |
| LIKELY-SAME | 48 |
| POSSIBLY-SAME | 130 |
| NOT-APPLICABLE | 165 |
| UNKNOWN | 625 |

Selftest recipes in `testSolidCraftParity()` still lock: #3541, #3578, #3526, #3528, #3573, #3561, #3587, #3563.

### LIKELY-SAME items moved by Fission since that pass

| Issue | Old note | Now (1.13.41–1.13.45) |
|---:|---|---|
| 2939 | Round face offset hard-fails | **Mostly handled** — curved Offset Face has OCCT offset + slab fallback; selftests on cylinder / taper |
| 3635, 2826, 3729, 928 | No To-Object / up-to-face | **Partial** — QML extrude: second face sets pull distance; C++ still has no To-Object / start-offset extent |
| 142, 319, 566 | Curved / saddle shell | **Improved** — hub shell, wall-area check, punch on curved openings, 15 s budget; saddles / rounds &lt; wall still open |
| (gap #6) | Shell `Build` unbounded | **Improved** — Shell worker + budget; fillet / chamfer / loft / sweep / revolve `Build` still unbounded |

The other **LIKELY-SAME** rows (fillet BRepCheck / volume guard, hidden-body targets, Combine hang, loft inner wires, thin/surface tools, pattern skip, setback fillet, …) are **still open** for Fission.

## New SolidCraft themes worth applying to Fission

Distinctive new open titles (generic “operation failed” stripped), grouped by what Fission should do. Ideas only.

### 1. Keep applying — same gaps, new echoes

| Theme | New SC examples | Fission action |
|---|---|---|
| Fillet / chamfer grows or tears the solid | #7720 angled cylinder fillet tears; #7770 colliding chamfers invalid; #7634 chamfer went hollow; #7334 overlapping blends; #7205 fillet bigger than the edges | After `IsDone()`, require volume not up + `BRepCheck`; reject or fit (#3435, #1080, #293) |
| Hidden / wrong boolean targets | #7520 hole drills hidden bodies; #7412 join to invisible features; #7654 join picker empty | Empty targets must skip hidden bodies (#2464, #4224, #4572) |
| Combine / many-tool hang | #7746 mirror+combine hang; #7497 combine of 20 sponges 12+ h | Time-limit Combine like other booleans (#2992, #3455, #1107) |
| Extrude To-Object / extents | #7780 To Object overshoots / To Offset ignored; #7494 To Object ignores moved plane; #7359 two-sides vs To Object dialog | Real start/end extents in `FissionExtrude` (#3635); QML distance-to-face is not enough |
| Slow text / tapered extrude | #7671 text cut ~2 min | Cap DraftAngle / glyph taper (#4431, #831) |
| Loft rings / centerline | #7702 can’t pick cylinder end; #7754 centerline/spine request; #7788 loft fails | Keep inner wires (#1184, #3111); optional spine (related #771) |
| Surface loft / thicken | #7599, #7383 surface loft | Still missing tools (#757, #765, #4358) |

### 2. New enough to log as Fission backlog chips

| Theme | New SC examples | Why it applies |
|---|---|---|
| Offset “Chain” | #7790 | Face/edge chain pick for Offset Face (Fusion-like) |
| Extrude past fillets / cone cuts | #7559 extrude on fillets; #7549 cut on cone | Dogfood + toolbench cases on imported curved topology |
| Pattern + timeline position | #7466 features not patternable unless timeline at end | Pattern should not require marker at tip |
| Coil dialog UX | #7481 revolutions spin buttons | Fission Coil is new (1.13.42–44); keep numeric steppers consistent |
| Measure on rounds | #7159 fillet/sphere face reports radius | Inspection: radius/diameter on analytic faces |
| Split by sketch | (still #1552) + new split failures | Split Body from sketch curves remains a clear gap |

### 3. Skip / truck / chrome (do not treat as Fission kernel work)

- Generic `FILLET: operation failed` storms without a public size recipe.
- Crash-between-commands templates tied to Rust workers / rayon (#7402, many Crash: …).
- Localization / ribbon glyph size / About markdown (#7776, #7721, #7586, #7504).
- GPU BAR mapping / NVIDIA Linux (#7663).
- Kernel swap / solidcore / usd-brep evaluation.

## Recommended Fission order (this week’s apply list)

Same priority as `solidcraft-parity-modeling.md`, refreshed. **Applied in 1.13.45** (clean-room): items 1–5 and pattern skip; loft rings are refused with a clear error (full hole loft still open); sweep/revolve/fillet Build timeouts and surface/thin tools remain.

1. **Fillet / chamfer validity** — volume + `BRepCheck` after blend; optional fit-to-geometry later. ✅ 1.13.45
2. **Hidden-body boolean filter** — extrude / hole / combine / loft join. ✅ 1.13.45 (empty targets)
3. **Combine time limit** — share the 20 s boolean cancel path. ✅ 1.13.45
4. **Extrude To-Object / start offset** — finish what 1.13.44’s face-distance gesture started. ✅ 1.13.45
5. **Loft inner loops** — stop turning rings into discs; consider centerline. ✅ refuse rings; centerline already existed
6. **Build() timeouts** — loft ✅; sweep / revolve / fillet still open (Shell already budgeted).
7. **Pattern instance skip** ✅ `skip` list; pattern while marker not at end still open.
8. **Surface / thin tools** — only if print-shop demand rises; larger feature surface.

## Method notes

- Counts: `gh api search/issues` on 2026-10-27 (UTC).
- New-issue sample: open issues with `created:>2026-10-21` matching modeling keywords (fillet, extrude, loft, …), deduped → 728.
- No SolidCraft repro files downloaded. No SolidCraft source read.
- “Apply” means Fission backlog / selftest recipes from the *failure mode*, not porting their fix.
