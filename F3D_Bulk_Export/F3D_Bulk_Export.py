# Fusion 360 Add-In: Export to Fission 360 (.f360)
#
# Does NOT walk the whole Data Panel (that freezes Fusion). Modes:
#   1) Active design          — currently open document
#   2) All open documents     — every open design
#   3) Active Data Panel folder — files in the folder shown in the Data Panel (not recursive)
#
# Each .f360 carries: parameters, timeline entries, sketch curves, construction planes,
# and a base64 STEP of the design (BRep) for Fission's import fallback. Per-step STEPs carry the body
# names and boxes of that step; mesh bodies (not in STEP) travel as raw triangles.
#
# Install: Scripts and Add-Ins → Add-Ins → + → this folder
#   or copy to %APPDATA%\Autodesk\Autodesk Fusion 360\API\AddIns\F3D_Bulk_Export\

import adsk.core, adsk.fusion
import traceback, json, os, time, tempfile, base64, re, struct

CMD_ID = 'F360_FissionExporter'
CMD_NAME = 'Export to Fission360'
CMD_DESC = 'Export designs to .f360 (STEP BRep + timeline + sketches)'
EXPORTER_VERSION = '1.6'

handlers = []
app = None
ui = None


def sanitize(name):
    return re.sub(r'[:\\/*?<>|"]', '_', name) or 'Untitled'


def point3(p):
    return [float(p.x), float(p.y), float(p.z)]


def design_units_name(design):
    try:
        u = design.unitsManager.defaultLengthUnits
        return 'in' if u in ('in', 'inch', 'ft', 'foot') else 'mm'
    except Exception:
        return 'mm'


# --------------- Geometry / timeline dump ------------------------------------

def hide_mesh_bodies(design):
    """Turns off visible mesh bodies and returns them. Fusion would otherwise write them into the STEP as
    faceted B-rep (one face per triangle); they travel separately in 'meshBodies'."""
    root = design.rootComponent
    comps, seen = [root], set()
    try:
        for occ in root.allOccurrences:
            c = occ.component
            if c.entityToken not in seen:
                seen.add(c.entityToken)
                comps.append(c)
    except Exception:
        pass
    hidden = []
    for c in comps:
        try:
            for b in c.meshBodies:
                try:
                    if b.isLightBulbOn:
                        b.isLightBulbOn = False
                        hidden.append(b)
                except Exception:
                    pass
        except Exception:
            pass
    return hidden


def show_bodies(bodies):
    for b in bodies:
        try:
            b.isLightBulbOn = True
        except Exception:
            pass


def export_step_b64(design, progress_cb=None):
    """One STEP for the whole design (B-rep bodies only) → base64. Empty string on failure."""
    tmp = os.path.join(tempfile.gettempdir(), '_fission_export_{}.stp'.format(os.getpid()))
    hidden = hide_mesh_bodies(design)
    try:
        if progress_cb:
            progress_cb('Writing STEP…')
        em = design.exportManager
        opts = em.createSTEPExportOptions(tmp)
        if not em.execute(opts):
            return ''
        with open(tmp, 'rb') as f:
            return base64.b64encode(f.read()).decode('ascii')
    except Exception:
        return ''
    finally:
        show_bodies(hidden)
        try:
            os.remove(tmp)
        except Exception:
            pass


def capture_thumbnail_b64(progress_cb=None):
    """PNG preview of the active viewport — embedded first in the .f360 for the Projects panel."""
    tmp = os.path.join(tempfile.gettempdir(), '_fission_thumb_{}.png'.format(os.getpid()))
    try:
        if progress_cb:
            progress_cb('Viewport preview…')
        vp = app.activeViewport
        if not vp:
            return ''
        try:
            vp.refresh()
        except Exception:
            pass
        # Fusion can reject tiny sizes; try a few
        ok = False
        for w, h in ((640, 400), (800, 500), (1024, 640), (320, 200)):
            try:
                if vp.saveAsImageFile(tmp, w, h):
                    ok = True
                    break
            except Exception:
                continue
        if not ok or not os.path.isfile(tmp) or os.path.getsize(tmp) < 32:
            return ''
        with open(tmp, 'rb') as f:
            return base64.b64encode(f.read()).decode('ascii')
    except Exception:
        return ''
    finally:
        try:
            os.remove(tmp)
        except Exception:
            pass


def write_f360(path, payload, thumbnail_b64=''):
    """Write .f360 with thumbnail as the first JSON key (Projects panel only reads the file head)."""
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    out = {}
    if thumbnail_b64:
        out['thumbnail'] = thumbnail_b64
    for k, v in payload.items():
        if k == 'thumbnail':
            continue
        out[k] = v
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=2)
    # The preview lives inside the .f360 only; remove sidecars left by exporter 1.3–1.4
    try:
        if os.path.exists(path + '.png'):
            os.remove(path + '.png')
    except Exception:
        pass


def sketch_to_dict(sketch):
    s = {
        'name': sketch.name,
        'isVisible': bool(sketch.isVisible),
        'timelineIndex': -1,
        'curves': [],
        'points': [],
        'dimensions': [],
        'constraints': [],
    }
    try:
        if sketch.timelineObject:
            s['timelineIndex'] = int(sketch.timelineObject.index)
    except Exception:
        pass
    try:
        plane = sketch.referencePlane
        if plane and hasattr(plane, 'geometry') and plane.geometry:
            g = plane.geometry
            s['plane'] = {'origin': point3(g.origin), 'normal': point3(g.normal)}
    except Exception:
        pass

    for crv in sketch.sketchCurves.sketchLines:
        try:
            s['curves'].append({
                'type': 'line',
                'construction': bool(crv.isConstruction),
                'start': point3(crv.startSketchPoint.geometry),
                'end': point3(crv.endSketchPoint.geometry),
            })
        except Exception:
            pass
    for crv in sketch.sketchCurves.sketchCircles:
        try:
            s['curves'].append({
                'type': 'circle',
                'construction': bool(crv.isConstruction),
                'center': point3(crv.centerSketchPoint.geometry),
                'radius': float(crv.radius),
            })
        except Exception:
            pass
    for crv in sketch.sketchCurves.sketchArcs:
        try:
            s['curves'].append({
                'type': 'arc',
                'construction': bool(crv.isConstruction),
                'center': point3(crv.centerSketchPoint.geometry),
                'start': point3(crv.startSketchPoint.geometry),
                'end': point3(crv.endSketchPoint.geometry),
                'radius': float(crv.radius),
            })
        except Exception:
            pass
    for pt in sketch.sketchPoints:
        try:
            s['points'].append(point3(pt.geometry))
        except Exception:
            pass
    for d in sketch.sketchDimensions:
        try:
            s['dimensions'].append({
                'type': str(d.objectType),
                'value': float(d.value) if hasattr(d, 'value') else None,
                'parameter': d.parameter.name if d.parameter else '',
            })
        except Exception:
            pass
    for c in sketch.geometricConstraints:
        try:
            s['constraints'].append(str(c.objectType))
        except Exception:
            pass
    return s


def _param_value_expr(param):
    """Return (value_mm_or_deg, expression_string) from a ModelParameter / ValueInput."""
    if param is None:
        return None, None
    try:
        val = float(param.value)
    except Exception:
        val = None
    expr = None
    try:
        expr = param.expression
    except Exception:
        pass
    return val, expr


def _set_mm(f, key, param):
    val, expr = _param_value_expr(param)
    if val is not None:
        f[key] = val
    if expr:
        f[key + 'Expr'] = expr


def feature_summary(feat):
    """Rich feature dump for Fission timeline reconstruction."""
    f = {
        'name': feat.name,
        'type': str(feat.objectType),
        'timelineIndex': -1,
    }
    try:
        if feat.timelineObject:
            f['timelineIndex'] = int(feat.timelineObject.index)
            f['isSuppressed'] = bool(feat.timelineObject.isSuppressed)
    except Exception:
        pass
    for attr in ('operation', 'extentType'):
        try:
            f[attr] = int(getattr(feat, attr))
        except Exception:
            pass

    # Profile → parent sketch name (Extrude / Revolve / Sweep / …)
    try:
        prof = feat.profile
        if prof:
            sketches = set()
            try:
                # ObjectCollection of profiles
                for i in range(prof.count):
                    p = prof.item(i)
                    if p and p.parentSketch:
                        sketches.add(p.parentSketch.name)
            except Exception:
                try:
                    if prof.parentSketch:
                        sketches.add(prof.parentSketch.name)
                except Exception:
                    pass
            if sketches:
                f['profileSketch'] = sorted(sketches)[0]
                f['profileSketches'] = sorted(sketches)
    except Exception:
        pass

    # Extent definitions (Extrude distance / taper)
    try:
        ext = feat.extentDefinition
        if ext:
            for attr in ('distance', 'distanceOne', 'distanceTwo', 'taperAngle'):
                try:
                    p = getattr(ext, attr, None)
                    if p is not None:
                        key = 'distance' if attr.startswith('distance') and attr != 'distanceTwo' else attr
                        if attr == 'distanceOne':
                            key = 'distance'
                        if attr == 'distanceTwo':
                            key = 'distance2'
                        _set_mm(f, key, p)
                except Exception:
                    pass
    except Exception:
        pass

    for val_attr in ('distance', 'angle', 'radius', 'thickness', 'taperAngle',
                     'diameter', 'depth', 'quantity', 'totalAngle'):
        if val_attr in f:
            continue
        try:
            v = getattr(feat, val_attr, None)
            if v is None:
                continue
            if hasattr(v, 'value') or hasattr(v, 'expression'):
                _set_mm(f, val_attr, v)
            else:
                f[val_attr] = float(v)
        except Exception:
            pass

    try:
        f['isOpposite'] = bool(feat.isOpposite)
    except Exception:
        pass
    return f


def walk_component(comp):
    out = {'features': [], 'bodies': [], 'sketches': [], 'construction': []}
    if not comp:
        return out

    for coll_name in (
        'extrudeFeatures', 'revolveFeatures', 'filletFeatures', 'chamferFeatures',
        'holeFeatures', 'sweepFeatures', 'loftFeatures', 'shellFeatures', 'ribFeatures',
        'circularPatternFeatures', 'rectangularPatternFeatures', 'mirrorFeatures',
        'moveFeatures', 'scaleFeatures', 'combineFeatures', 'draftFeatures',
        'offsetFacesFeatures', 'replaceFaceFeatures', 'splitFaceFeatures',
        'splitBodyFeatures', 'threadFeatures',
    ):
        try:
            coll = getattr(comp.features, coll_name, None)
            if not coll:
                continue
            for feat in coll:
                out['features'].append(feature_summary(feat))
        except Exception:
            pass

    for body in comp.bRepBodies:
        try:
            b = {
                'name': body.name,
                'area': float(body.area),
                'volume': float(body.volume),
                'isSolid': bool(body.isSolid),
                'faceCount': int(body.faces.count),
            }
            out['bodies'].append(b)
        except Exception:
            pass

    for sketch in comp.sketches:
        try:
            out['sketches'].append(sketch_to_dict(sketch))
        except Exception:
            pass

    for plane in comp.constructionPlanes:
        try:
            p = {'name': plane.name}
            g = plane.geometry
            if g:
                p['origin'] = point3(g.origin)
                p['normal'] = point3(g.normal)
            out['construction'].append(p)
        except Exception:
            pass

    return out


def merge_walk(dst, src):
    for k in dst:
        if isinstance(dst[k], list):
            dst[k].extend(src.get(k, []))


SNAPSHOT_SKIP = ('sketch', 'constructionplane', 'constructionaxis', 'constructionpoint')


def count_bodies(design):
    root = design.rootComponent
    n = root.bRepBodies.count
    try:
        for occ in root.allOccurrences:
            n += occ.component.bRepBodies.count
    except Exception:
        pass
    return n


def _box_mm(body):
    bb = body.boundingBox
    lo, hi = bb.minPoint, bb.maxPoint
    return [round(v * 10.0, 4) for v in (lo.x, lo.y, lo.z, hi.x, hi.y, hi.z)]


def body_list(design):
    """[{name, box}] for every B-rep body in world coordinates (mm). Fission names the solids of a step
    STEP by matching these boxes when the STEP itself carries no body names."""
    root = design.rootComponent
    found = []
    for b in root.bRepBodies:
        try:
            found.append((b.name, root.name, _box_mm(b)))
        except Exception:
            pass
    try:
        for occ in root.allOccurrences:
            for b in occ.bRepBodies:   # proxies: boxes are in the root's coordinates
                try:
                    found.append((b.name, occ.component.name, _box_mm(b)))
                except Exception:
                    pass
    except Exception:
        pass
    uses = {}
    for name, _, _ in found:
        uses[name] = uses.get(name, 0) + 1
    out = []
    for name, comp, box in found:
        out.append({'name': '{} ({})'.format(name, comp) if uses[name] > 1 else name, 'box': box})
    return out


def _mesh_arrays(body):
    """(node coordinates in cm as a flat list, triangle node indices as a flat list) of a MeshBody."""
    try:
        pm = body.mesh
        coords = list(pm.nodeCoordinatesAsDouble)
        tris = list(pm.triangleNodeIndices)
        quads = list(pm.quadNodeIndices)
        for k in range(0, len(quads) - 3, 4):
            a, b, c, d = quads[k:k + 4]
            tris += [a, b, c, a, c, d]
        try:
            poly = list(pm.polygonNodeIndices)
            pos = 0
            for n in list(pm.nodeCountPerPolygon):
                ids = poly[pos:pos + n]
                pos += n
                for j in range(1, n - 1):
                    tris += [ids[0], ids[j], ids[j + 1]]
        except Exception:
            pass
        if coords and tris:
            return coords, tris
    except Exception:
        pass
    tm = body.displayMesh
    return list(tm.nodeCoordinatesAsDouble), list(tm.nodeIndices)


def mesh_body_to_dict(body, comp_name, transform=None):
    coords, tris = _mesh_arrays(body)
    if not coords or not tris:
        return None
    m = None
    if transform is not None:
        try:
            m = list(transform.asArray())
            if m == [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]:
                m = None
        except Exception:
            m = None
    mm = []
    for k in range(0, len(coords) - 2, 3):
        x, y, z = coords[k], coords[k + 1], coords[k + 2]
        if m:
            x, y, z = (m[0] * x + m[1] * y + m[2] * z + m[3],
                       m[4] * x + m[5] * y + m[6] * z + m[7],
                       m[8] * x + m[9] * y + m[10] * z + m[11])
        mm += [x * 10.0, y * 10.0, z * 10.0]   # Fusion's API works in cm
    tris = tris[:len(tris) - len(tris) % 3]
    try:
        visible = bool(body.isLightBulbOn)
    except Exception:
        visible = True
    return {
        'name': body.name,
        'component': comp_name,
        'isVisible': visible,
        'nodeCount': len(mm) // 3,
        'triangleCount': len(tris) // 3,
        'nodes': base64.b64encode(struct.pack('<%df' % len(mm), *mm)).decode('ascii'),
        'triangles': base64.b64encode(struct.pack('<%di' % len(tris), *tris)).decode('ascii'),
    }


def collect_mesh_bodies(design, progress_cb=None):
    """Mesh bodies are not part of a STEP export; they travel as raw triangles (mm, world coordinates)."""
    root = design.rootComponent
    out = []
    sources = [(b, root.name, None) for b in root.meshBodies]
    try:
        for occ in root.allOccurrences:
            for b in occ.component.meshBodies:
                sources.append((b, occ.component.name, occ.transform2))
    except Exception:
        pass
    for i, (body, comp_name, transform) in enumerate(sources):
        if progress_cb:
            progress_cb('Mesh body {}/{}…'.format(i + 1, len(sources)))
        try:
            d = mesh_body_to_dict(body, comp_name, transform)
            if d:
                out.append(d)
        except Exception:
            pass
    return out


def export_step_snapshots(design, timeline, progress_cb=None):
    """Rolls the timeline marker to just after each solid step and stores a STEP of the model there
    (entry['stepData']), or entry['stepEmpty'] when no bodies exist yet. Restores the marker."""
    tl = design.timeline
    original = tl.markerPosition
    any_snapshot = False
    try:
        for i, entry in enumerate(timeline):
            kind = (entry.get('type') or '').lower()
            if any(s in kind for s in SNAPSHOT_SKIP) or entry.get('isSuppressed'):
                continue
            if progress_cb:
                progress_cb('BRep step {}/{}…'.format(i + 1, len(timeline)))
            try:
                tl.markerPosition = i + 1
            except Exception:
                continue
            if count_bodies(design) == 0:
                entry['stepEmpty'] = True
                continue
            b64 = export_step_b64(design)
            if b64:
                entry['stepData'] = b64
                entry['stepBodies'] = body_list(design)
                any_snapshot = True
    finally:
        try:
            tl.markerPosition = original
        except Exception:
            pass
    return any_snapshot


def export_document_to_f360(doc, out_path, include_step=True, unhide=False, progress_cb=None, per_step=True):
    """Build a Fusion-export .f360 for one open Document.
    Returns (ok: bool, had_thumbnail: bool)."""
    design = adsk.fusion.Design.cast(doc.products.itemByProductType('DesignProductType'))
    if not design:
        design = adsk.fusion.Design.cast(app.activeProduct)
    if not design:
        return False, False

    if unhide:
        try:
            unhide_all(design.rootComponent)
        except Exception:
            pass

    root = design.rootComponent
    payload = {
        'format': 'Fission360',
        'version': 1,
        'document': {
            'name': doc.name,
            'units': design_units_name(design),
        },
        'parameters': {},
        'timeline': [],
        'features': [],
        'bodies': [],
        'sketches': [],
        'construction': {'planes': []},
        'metadata': {
            'exportedFrom': 'Fusion 360',
            'exporterVersion': EXPORTER_VERSION,
            'exportedAt': time.strftime('%Y-%m-%dT%H:%M:%S'),
            'source': 'F3D_Bulk_Export',
        },
    }

    if progress_cb:
        progress_cb('Parameters…')
    for param in design.userParameters:
        try:
            payload['parameters'][param.name] = {
                'value': float(param.value),
                'expression': param.expression,
                'unit': param.unit,
                'comment': param.comment if hasattr(param, 'comment') else '',
            }
        except Exception:
            pass

    if progress_cb:
        progress_cb('Timeline…')
    tl = design.timeline
    for i in range(tl.count):
        try:
            obj = tl.item(i)
            entry = {
                'index': i,
                'type': str(obj.entity.objectType) if obj.entity else str(obj.objectType),
                'name': obj.name if hasattr(obj, 'name') else '',
                'isSuppressed': bool(obj.isSuppressed) if hasattr(obj, 'isSuppressed') else False,
            }
            payload['timeline'].append(entry)
        except Exception:
            payload['timeline'].append({'index': i, 'type': 'unknown', 'name': ''})

    if progress_cb:
        progress_cb('Features / sketches…')
    data = walk_component(root)
    try:
        for occ in root.allOccurrences:
            merge_walk(data, walk_component(occ.component))
    except Exception:
        for occ in root.occurrences:
            merge_walk(data, walk_component(occ.component))

    payload['features'] = data['features']
    payload['bodies'] = data['bodies']
    payload['sketches'] = data['sketches']
    payload['construction']['planes'] = data['construction']

    try:
        payload['meshBodies'] = collect_mesh_bodies(design, progress_cb)
    except Exception:
        payload['meshBodies'] = []

    # BRep after each solid timeline step, so scrubbing the timeline in Fission shows the model growing
    has_snapshots = False
    if per_step:
        has_snapshots = export_step_snapshots(design, payload['timeline'], progress_cb)

    # Full-design STEP as BRep carrier (Fission load → importStepFallback); not needed with snapshots
    if include_step and not has_snapshots:
        step_b64 = export_step_b64(design, progress_cb)
        if step_b64:
            if payload['bodies']:
                payload['bodies'][0]['stepData'] = step_b64
            else:
                payload['bodies'].append({
                    'name': sanitize(doc.name),
                    'isSolid': True,
                    'stepData': step_b64,
                })
            # Avoid duplicating huge STEP on every body
            for b in payload['bodies'][1:]:
                b.pop('stepData', None)

    thumb = capture_thumbnail_b64(progress_cb)
    if progress_cb:
        progress_cb('Writing file…')
    write_f360(out_path, payload, thumb)
    return True, bool(thumb)


def unhide_all(comp):
    try:
        comp.isBodiesFolderLightBulbOn = True
    except Exception:
        pass
    try:
        comp.isSketchFolderLightBulbOn = True
    except Exception:
        pass
    for body in comp.bRepBodies:
        try:
            body.isLightBulbOn = True
        except Exception:
            pass
    try:
        for body in comp.meshBodies:
            try:
                body.isLightBulbOn = True
            except Exception:
                pass
    except Exception:
        pass
    for occ in comp.occurrences:
        try:
            occ.isLightBulbOn = True
        except Exception:
            pass
        try:
            unhide_all(occ.component)
        except Exception:
            pass


# --------------- Collect targets (no deep cloud walks) -----------------------

def open_design_documents():
    docs = []
    for i in range(app.documents.count):
        d = app.documents.item(i)
        if not d:
            continue
        if adsk.fusion.Design.cast(d.products.itemByProductType('DesignProductType')):
            docs.append(d)
    return docs


def datafiles_in_active_folder():
    """Shallow list of .f3d/.f3z in the Data Panel's current folder only."""
    folder = app.data.activeFolder
    if not folder:
        return []
    out = []
    for i in range(folder.dataFiles.count):
        df = folder.dataFiles.item(i)
        if not df:
            continue
        ext = (df.fileExtension or '').lower()
        if ext in ('f3d', 'f3z'):
            out.append(df)
    return out


# --------------- Command UI --------------------------------------------------

class IDs:
    output_dir = 'output_dir'
    browse_btn = 'browse_btn'
    mode = 'mode'
    unhide_all = 'unhide_all'
    include_step = 'include_step'
    per_step = 'per_step'
    overwrite = 'overwrite'


class InputChangedHandler(adsk.core.InputChangedEventHandler):
    def notify(self, args):
        try:
            changed = args.input
            inputs = args.inputs
            if changed.id != IDs.browse_btn:
                return
            # Reset the momentary button
            try:
                changed.value = False
            except Exception:
                pass
            dlg = ui.createFolderDialog()
            dlg.title = 'Fission 360 export folder'
            if dlg.showDialog() != adsk.core.DialogResults.DialogOK:
                return
            inputs.itemById(IDs.output_dir).value = dlg.folder
        except Exception:
            pass


class ExecuteHandler(adsk.core.CommandEventHandler):
    def notify(self, args):
        try:
            inputs = args.command.commandInputs
            out_dir = (inputs.itemById(IDs.output_dir).value or '').strip()
            if not out_dir:
                ui.messageBox('Choose an output folder first.')
                return
            os.makedirs(out_dir, exist_ok=True)

            mode = inputs.itemById(IDs.mode).selectedItem.name
            unhide = inputs.itemById(IDs.unhide_all).value
            include_step = inputs.itemById(IDs.include_step).value
            per_step = inputs.itemById(IDs.per_step).value
            overwrite = inputs.itemById(IDs.overwrite).value

            jobs = []  # list of ('open', Document) or ('data', DataFile)

            if mode.startswith('Active design'):
                doc = app.activeDocument
                if not doc or not adsk.fusion.Design.cast(
                        doc.products.itemByProductType('DesignProductType')):
                    ui.messageBox('Open a Design document first.')
                    return
                jobs.append(('open', doc))
            elif mode.startswith('All open'):
                jobs = [('open', d) for d in open_design_documents()]
                if not jobs:
                    ui.messageBox('No open Design documents.')
                    return
            else:
                files = datafiles_in_active_folder()
                if not files:
                    ui.messageBox(
                        'No .f3d / .f3z files in the active Data Panel folder.\n'
                        'Select a folder in the Data Panel, then run again.')
                    return
                jobs = [('data', df) for df in files]

            progress = ui.createProgressDialog()
            progress.cancelButtonText = 'Cancel'
            progress.isBackgroundTranslucent = False
            progress.isCancelButtonShown = True
            progress.show('Export to Fission360', 'Starting…', 0, len(jobs), 0)

            results = {'ok': [], 'skip': [], 'err': []}

            for idx, (kind, target) in enumerate(jobs):
                if progress.wasCancelled:
                    break
                progress.progressValue = idx

                close_after = False
                doc = None
                label = ''
                try:
                    if kind == 'open':
                        doc = target
                        label = doc.name
                    else:
                        df = target
                        label = '{}.{}'.format(df.name, df.fileExtension)
                        progress.message = '({}/{}) Opening {}'.format(idx + 1, len(jobs), label)
                        adsk.doEvents()
                        doc = app.documents.open(df, True)
                        close_after = True
                        if not doc:
                            results['err'].append(label + ': open failed')
                            continue

                    out_path = os.path.join(out_dir, sanitize(doc.name) + '.f360')
                    if os.path.exists(out_path) and not overwrite:
                        results['skip'].append(label)
                        continue

                    def cb(msg, _label=label, _idx=idx):
                        progress.message = '({}/{}) {}: {}'.format(_idx + 1, len(jobs), _label, msg)
                        adsk.doEvents()

                    progress.message = '({}/{}) Exporting {}'.format(idx + 1, len(jobs), label)
                    adsk.doEvents()

                    try:
                        doc.activate()
                    except Exception:
                        pass
                    try:
                        app.activeViewport.fit()
                    except Exception:
                        pass
                    adsk.doEvents()

                    ok, had_thumb = export_document_to_f360(
                        doc, out_path,
                        include_step=include_step,
                        unhide=unhide,
                        progress_cb=cb,
                        per_step=per_step)
                    if ok:
                        results['ok'].append(label + ('' if had_thumb else ' (no preview)'))
                        if had_thumb:
                            results.setdefault('thumbs', []).append(label)
                    else:
                        results['err'].append(label + ': not a Design / export failed')
                except Exception as e:
                    results['err'].append('{}: {}'.format(label or '?', e))
                finally:
                    if close_after and doc:
                        try:
                            doc.close(False)
                        except Exception:
                            pass

            progress.hide()

            n_thumb = len(results.get('thumbs', []))
            msg = (
                'Export finished\n\n'
                'OK: {}\nWith viewport preview: {}\nSkipped (exists): {}\nErrors: {}\n\n'
                'Folder:\n{}'
            ).format(len(results['ok']), n_thumb, len(results['skip']), len(results['err']), out_dir)
            if results['err']:
                msg += '\n\n' + '\n'.join(results['err'][:12])
            ui.messageBox(msg)
        except Exception:
            ui.messageBox('Error:\n{}'.format(traceback.format_exc()))


class DestroyHandler(adsk.core.CommandEventHandler):
    def notify(self, args):
        try:
            adsk.terminate()
        except Exception:
            pass


class CreatedHandler(adsk.core.CommandCreatedEventHandler):
    def notify(self, args):
        try:
            cmd = args.command
            cmd.isExecutedWhenPreEmpted = False
            inputs = cmd.commandInputs

            inputs.addStringValueInput(
                IDs.output_dir, 'Output folder',
                os.path.join(os.path.expanduser('~'), 'Documents', 'Fission 360 Export'))
            inputs.addBoolValueInput(IDs.browse_btn, 'Browse…', False, '', False)

            mode = inputs.addDropDownCommandInput(
                IDs.mode, 'Export', adsk.core.DropDownStyles.TextListDropDownStyle)
            mode.listItems.add('Active design', True)
            mode.listItems.add('All open designs', False)
            mode.listItems.add('Active Data Panel folder (this folder only)', False)

            grp = inputs.addGroupCommandInput('opts', 'Options')
            grp.children.addBoolValueInput(IDs.include_step, 'Include STEP (BRep for Fission)', True, '', True)
            grp.children.addBoolValueInput(IDs.per_step, 'BRep per timeline step (scrubbing; slower, larger)', True, '', True)
            grp.children.addBoolValueInput(IDs.unhide_all, 'Unhide all before export', True, '', False)
            grp.children.addBoolValueInput(IDs.overwrite, 'Overwrite existing .f360', True, '', False)

            inputs.addTextBoxCommandInput(
                'hint', '',
                'Tip: for many files, open the folder in the Data Panel and use '
                '"Active Data Panel folder". Full project trees are not scanned '
                '(that freezes Fusion).',
                3, True)

            on_ex = ExecuteHandler()
            cmd.execute.add(on_ex)
            handlers.append(on_ex)
            on_in = InputChangedHandler()
            cmd.inputChanged.add(on_in)
            handlers.append(on_in)
            on_ds = DestroyHandler()
            cmd.destroy.add(on_ds)
            handlers.append(on_ds)
        except Exception:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))


def run(context):
    global app, ui, handlers
    try:
        handlers = []
        app = adsk.core.Application.get()
        ui = app.userInterface

        cmd_def = ui.commandDefinitions.itemById(CMD_ID)
        if cmd_def:
            cmd_def.deleteMe()

        cmd_def = ui.commandDefinitions.addButtonDefinition(CMD_ID, CMD_NAME, CMD_DESC)
        on_created = CreatedHandler()
        cmd_def.commandCreated.add(on_created)
        handlers.append(on_created)
        cmd_def.execute()
        adsk.autoTerminate(False)
    except Exception:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))


def stop(context):
    global handlers
    try:
        ui_ = adsk.core.Application.get().userInterface
        cmd_def = ui_.commandDefinitions.itemById(CMD_ID)
        if cmd_def:
            cmd_def.deleteMe()
    except Exception:
        pass
    handlers = []
