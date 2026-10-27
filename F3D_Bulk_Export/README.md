# F3D Bulk Export — companion add-in for Fission 360

Python add-in that writes **`.f360`** files Fission can open: timeline, sketches, construction, STEP B-rep, optional per-step solids, mesh bodies, and a viewport preview.

**Current add-in:** **1.10** (`EXPORTER_VERSION` in `F3D_Bulk_Export.py` must match `"version"` in `F3D_Bulk_Export.manifest`).

## Install

1. Fusion → **Utilities → Add-Ins → Scripts and Add-Ins**.
2. **Add-Ins** tab → **+** → pick this folder (`F3D_Bulk_Export`), or copy it to  
   `%APPDATA%\Autodesk\Autodesk Fusion 360\API\AddIns\F3D_Bulk_Export\`.
3. Run **Export to Fission360** from the add-ins list.

## What it exports

| Mode | Scope |
|------|--------|
| Active design | The open document |
| All open documents | Every open design |
| Active Data Panel folder | Files in the folder shown in the Data Panel (**not** recursive — a full tree walk freezes Fusion) |

Options typically include Include STEP, BRep per timeline step, Unhide all, and Overwrite. Linked designs: export each design into the matching folder under Fission’s projects tree so **Linked Designs** can find them.

## Open in Fission

File → Open the `.f360`, or drop it on the Projects panel. Prefer add-in **1.10+** so units and sketch frames land at the right size. Details: [`../FISSION_360_PROJECT.md`](../FISSION_360_PROJECT.md) (Fusion import) and [`../README.md`](../README.md).
