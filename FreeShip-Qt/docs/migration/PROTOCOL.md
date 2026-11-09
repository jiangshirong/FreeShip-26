# Model service contract, version 1

The production backend is now FreeShipDocumentService.exe, which owns committed
FBM states, document identity/revision, undo/redo and savepoints. The Pascal child
is a working calculation adapter. C++ publishes immutable snapshots; no editable
model is reconstructed from FSLP screen primitives.

Start `FreeShipDocumentService.exe --serve [parent_pid]` in a private temporary directory.
`FreeShipGeometryBridge.exe --serve` remains available for legacy oracle tests.
UTF-8 JSON requests and responses are one line each, through stdin/stdout. Each
request contains integer `id`, string `method`. Responses echo `id`, `ok`; errors
return `error` without a modal dialog. Ignore diagnostic non-JSON stdout lines.

Methods:

- `load`: `path` private FBM copy, `document_id` UUID, `output` private directory.
  On success resets undo history and emits a complete snapshot. Failed loads keep
  the previous document. Revision is positive and monotonic within a document.
- `move_point`: `point_id`, `position:[x,y,z]`, `expected_document_id`, `expected_revision`, `output`.
  Reject unknown/locked IDs, stale revisions and non-finite/unbounded values.
  One command moves one point; no-op coordinates do not create an undo step.
- `undo`, `redo`: `expected_document_id`, `expected_revision`, `output`. History state is backend-owned.
- `snapshot`: `expected_document_id`, `expected_revision`, `output`, emit the current document without editing it.
- `save`: `expected_document_id`, `expected_revision`, `output`, writes `saved.fbm` in this private
  directory. Returns `saved_file` and `save_receipt`; does not mark clean before
  the UI's atomic destination write has succeeded.
- `mark_saved`: `expected_document_id`, `expected_revision`, the unchanged
  `save_receipt`, and `saved_path` of the installed file. Verifies the prepared
  revision and actual destination bytes before marking that history state clean.
  Missing, superseded and stale receipts are rejected without changing history
  or deleting recovery data. Returns `save_confirmed:true`, `save_receipt` and
  the normalized `saved_path` on success.
- `quit`: respond and exit.

Since 2026-11-09 the native service requires a receipt for save confirmation.
Receipt fields are `version:1`, a fresh UUID `token`, `document_id`, `revision`,
`bytes` and lowercase SHA-256 `sha256` of the prepared FBM. The caller must not
modify this object. The GUI also checks these promised bytes while staging its
destination transaction; changing the private prepared file cannot substitute
different bytes between receipt validation and installation.

Confirmation of the latest completed receipt is idempotent for the same target
and revision. It never rewrites the file or rotates its backup. A new preparation
replaces the previous receipt; document/revision changes invalidate it. Read-only
requests and rejected stale commands leave a valid preparation available. Qt
retries an unanswered confirmation up to twice at three-second intervals, using
the same request ID and receipt. Late duplicate replies cannot overwrite newer
UI state. A dead service is still reported immediately and requires reopening;
receipts are not a durable undo-history journal. The isolated Pascal oracle
retains its original confirmation protocol.

`save` additionally accepts optional `preview_jpeg` (base64, at most 700000 characters).
The decoded JPEG must be 400x300 and at most 512 KiB. Only an enabled preview in a
supported file without opaque extensions is replaced; the rest of the FBM stays exact.
It returns `preview_updated`. The prepared image is adopted only by `mark_saved`
for the same saved-state object and revision; a failed/replaced preparation is not committed.
State responses include `preview_enabled`, allowing the GUI to skip rendering for
disabled/unsupported files. A normal save without this option remains byte preserving.

M2 editing additions use the same document/revision guards and private output:

Native document dispatch executes `move_point` and `move_points` in C++ and
returns `edit_engine: "cpp-points"`, `fbm_write_engine: "cpp-coordinate-writer"`,
`point_edit_ms` and `adapter_operation` (`apply_document_points` or `snapshot`).
The point time covers validation, dependency propagation and FBM patching; it
does not include display/analysis reconstruction or total GUI response time.
In external-document mode the Pascal adapter rejects direct point-edit commands.
Coordinate candidates invalidate serialized section and flowline caches while
preserving their definitions. A no-op preserves original bytes and redo state.
The private `apply_document_points` adapter operation accepts the complete native
checkpoint plus changed absolute coordinates. It updates the existing work copy
without propagating constraints again or reloading/re-encoding the whole FBM.

- `auxiliary`, `operation=create_control`, `ids:[edge_id,...]`: validate the
  entire nonempty unique selection, ignore already assigned edges, and trace
  open chains or closed loops. Disconnected chains become separate definitions.
  Degree above two is rejected before editing. No eligible edges is a no-op.
- `auxiliary`, `operation=create_flow`, `view:0|1|2`, `source:[u,v]`: source
  coordinates are body Y/Z, profile X/Z, or plan X/Y. Follow the original hull
  trace; fewer than two points is rejected without committing history.
- `auxiliary`, `operation=delete`, `kind:curve|flow`, `ids:[id,...]`: remove
  definitions, retaining control points/edges/faces. These IDs are scoped by
  kind, survive undo/redo, and are not reused after deletion or history branches.
- Optional model arrays `control_curves` and `flowlines` each allow at most 4096
  entries. Both have positive unique `id`, `name`, BGR `color`, and `spline`
  with matching `points` and Boolean `knuckles`. Total controls are capped at
  one million. Control curves add `controls` referencing existing point IDs;
  flowlines add the source `view` and `source`. Empty stored flow traces remain
  representable after a hull edit invalidates their intersection.
- Curve definitions require FBM 1.95; flowline creation/visibility require 2.50.
  Geometrical edits explicitly invalidate flow caches even when `Surface.Built`
  is already false. Camera movement and visibility changes reuse their data.

- `display`: `changes` patches `both_sides`, `show_curvature`,
  `show_control_curves`, `show_flowlines` (bool),
  `curvature_scale` (finite Single, 0..1e9). Reject unknown fields. Stored in the
  original FBM visibility block with undo/redo; skip surface and hydro rebuild.
  Snapshot `display` contains these values plus `curvature_color`. Optional in
  old synthetic snapshots; production always emits it.
- `FreeShipSectionDisplay` v1 now includes optional `splines` parallel to each
  section's sampled `paths`, with `points` and `knuckles`. They are independent
  copies of the original native cut, for lazy curvature presentation. Validate
  matching counts and bound controls separately from sampled path vertices.
  Worker executable hashes invalidate persistent cache entries after upgrades.

- `update_project`: `changes` is a patch of `name`, `designer`, `comment`,
  `units`, `length`, `beam`, `draft`, `water_density`, `appendage_coefficient`,
  `split_section_location`, `use_default_split_section_location`,
  `simplify_intersections`, `precision` (0..3), `shade_underwater`,
  `underwater_color`, `underwater_color_alpha` (integer 0..255; RGB tint strength,
  not layer transparency), `save_preview`, `coefficient_basis` (0=project dimensions,
  1=actual submerged dimensions), and Boolean `hydro_displacement`,
  `hydro_lateral_area`, `hydro_sectional_areas`, `hydro_metacentric_height`,
  `hydro_lcf`, and Boolean `generate_auxiliary_reports`. The five feature switches preserve the master visibility flag.
  These persisted values are also published under snapshot `settings`. Mixed
  changes commit as one undo entry; absent old fields trigger format promotion
  only if their effective value changes. Unknown keys are errors. Water temperature is not offered:
  the inherited FBM reader/writer does not persist it.
- `update_layer`: `layer_id`, `changes` containing only `name`, `color`,
  `visible`, `symmetric`, `developable`, `use_in_hydrostatics`,
  `show_in_linesplan`, `use_for_intersections`, `thickness`, `material_density`.
  Visibility, linesplan inclusion and intersection inclusion remain independent.
  Enabling hydrostatics uses the inherited automatic symmetry rule; an explicit
  symmetry choice in the same command is applied last.
- `move_points`: `moves:[{point_id,position:[x,y,z]},...]`. One atomic movement,
  one history step. All points must be unlocked and free of anchor or
  linear-constraint dependencies, including reverse dependencies. A rejected
  point rejects the complete batch. Single-point edits retain the original
  dependency propagation through `move_point`. Each snapshot point exposes
  `batch_movable` from the same dependency check. Views select only visible
  points; visibility is a presentation filter, not an API editing permission.

All changes are validated transactionally before publication. Numeric no-ops are compared in the
original Single domain and preserve redo history. Generation failure rolls back
the complete edit, including metadata and file version. Metadata edits upgrade
old FBM versions only as needed to retain the edited properties when saved.
Text patches must round-trip through the current FBM text encoding; otherwise
the complete edit fails instead of replacing characters with question marks.
`settings.text_encoding` identifies this limitation; it is not an encoding tag
stored in the old FBM format.

Background image commands use the same document/revision guards and history:

- `background`, `view:0|1|2` (body/profile/plan), `operation:import|update|remove`.
  `import` requires a private validated JPEG `path` and positive `scale`; the Qt
  client converts BMP once before sending this command. Decode failure preserves
  the previous image. Importing into an early FBM promotes to at least `fv250`.
- `update` requires `changes` containing only `origin:[pixelX,pixelY]`, `scale`,
  `visible`, `alpha`, `transparent`, `transparent_color`, or `tolerance`.
  Origins are integer image pixels, scale is the original Single model-units/pixel.
  Alpha and tolerance are integers 0..255; transparent_color is BGR24 as elsewhere.
  Scale is finite in 1e-9..1e9 and each origin coordinate is bounded by 1e9.
- Model snapshots contain optional `backgrounds` (at most one per orthographic
  view), with all the above fields, `quality:1..100`, and canonical base64 `jpeg`.
  Raw JPEG data remains unchanged through metadata edits, history and FBM saves.
  Images are bounded to 32 MiB encoded, 16384 pixels per dimension and 32 Mi pixels.
  Absence of the array means no background, for earlier snapshots.
- Background edits do not invalidate hull subdivision or hydrostatic analysis.
  Full artifacts still use the existing publication/rollback boundary. Geometry
  content cache hits preserve complete scene and DXF output; camera and gesture
  preview never enter the saved model.

Project unit changes convert the full model using the original unit setter,
including dependent engineering settings. Explicit numeric patch fields are
expressed in the **target** units and applied after conversion. A pure unit
change therefore sends only `units`. Layer thickness uses mm/in, and material
density uses t/m3 or lb/ft3 (not long tons). Background image scale and flowline
projection seeds are model lengths and are converted too; pixel origin is not.
Changing project length/beam/draft without changing units does not scale the hull.

Opening or cancelling a property editor does not send a model command. Untouched
values retain their original precision and text; display formatting cannot edit
the model. A batch coordinate inspector edits translation deltas, while a single
point inspector edits absolute coordinates. Moving selected points preserves the
axis hidden by an orthographic projection.

Successful state responses include `document_id`, `revision`, `can_undo`,
`can_redo`, `modified`. Snapshot-producing methods also return absolute paths in
`model`, `scene`, `dxf` to `model.json`, `scene.fslp`, `reference.dxf` respectively.
Client validates all artifacts before publishing a revision to any view.
Document commands reject an incorrect document ID even when the revision and
entity IDs happen to match. An editing or load operation that fails while
generating its artifacts restores the previous model and undo history.

The service bounds IDs and revisions to exact JSON integers, 1 through 2^53-1.
Coordinates and project dimensions are finite and bounded by 1e9 in magnitude;
length, beam and draft must be nonnegative. Artifact limits are enforced before
adopting a loaded document. The C++ reader independently checks the contract.
Requests are serialized by the client; only one may be outstanding.

Optional diagnostics `history_states`, `history_bytes`, and `history_limit_bytes`
describe the backend undo budget. The existing preference controls the budget,
with the current and preceding state retained even if those two exceed it.
Evicting a saved history position must not mark another position clean.

The client retains only the current artifact directory and current input copy.
An existing destination is replaced through QSaveFile without direct-write
fallback. If the destination was committed but acknowledgement fails, the client
keeps that new path and reports the distinction while retaining modified state.

`model.json`:

```
{
 "format":"FreeShipModelSnapshot", "version":1,
 "document_id":"...", "revision":1,
 "name":"...", "units":"metric",
 "bounds":{"min":[0,0,0],"max":[20,4,8]},
 "settings":{"length":20,"beam":8,"draft":3,"waterline_z":3,
             "shade_underwater":true,"underwater_color":8388608},
 "control_points":[{"id":1,"p":[0,0,0],"locked":false}],
 "control_edges":[{"id":1,"a":1,"b":2,"crease":true}],
 "control_faces":[{"id":1,"layer":1,"points":[1,2,3,4]}],
 "layers":[{"id":1,"name":"Hull","color":32768,"visible":true,"symmetric":true}],
 "triangles":[{"layer":1,"p":[[0,0,0],[1,0,0],[0,1,0]]}]
}
```

Colors use the existing Windows RGB integer representation. Coordinates use the
document's original units, with no numerical resampling or rounding beyond the
original core's float type. Triangles are the original-side derived mesh; mirror
only layers flagged symmetric. `waterline_z` is the absolute hull min Z + draft.

IDs are stable through coordinate and supported topology edits, undo and redo.
They are scoped by `document_id` and entity kind. Deleted IDs are not reused;
arrays contain only living objects, so their IDs may have gaps. Cross-file ID
persistence is not promised by this format.

FreeShip UI units labels: metric => m; imperial => ft. A failed save does not
clear modified state. Generation precedes DXF output, matching the accepted
pilot's spline sampling lifecycle. Export uses the complete model, never camera
clipping or an interaction preview.

## Native lines-plan generation

Production Qt requests set `native_linesplan: true`. The service exports a
`FreeShipLinesplanInput` version 1 control net plus section definitions, palette,
units, draft and separate display/export split positions. The C++ worker generates
the complete FSLP and DXF; the legacy scene generator is not called on this route.
The request field defaults to false for explicit legacy reference tests.

Native snapshot responses additionally return `native_report`, an artifact in
the same private request directory. Before publishing, Qt requires `status=pass`,
`engine=cpp-native`, matching document ID/revision, and exact `scene_sha256` and
`dxf_sha256`. Native responses also include `sections` (sections.json), bound by
`sections_sha256` in the same report. All files are staged before a successful response. Worker failure
uses the existing model/history rollback; the original snapshot remains usable.

The worker may reuse a content-addressed cache within the private model session.
The key includes input content and worker binary hash, excluding only document ID
and revision, which are rebound in the new output report. Cached outputs must pass
hash and format checks. No cross-application-restart persistence is promised.

## Topology commands

`method=topology` uses the same expected document/revision and private output
contract. `operation` is one of:

- `add_point`: `position=[x,y,z]` in document units.
- `split_edge`: `edge_id`; insert the Single-representable midpoint.
- `set_crease`: nonempty unique edge `ids`, Boolean `crease`.
- `set_locked`: nonempty unique point `ids`, Boolean `locked`.
- `add_face`: ordered unique point `ids`, `layer_id`.
- `assign_layer`: nonempty unique face `ids`, `layer_id`.
- `delete`: `kind=point|edge|face`, nonempty unique typed `ids`.

Deletion can remove dependent edges/faces according to original model rules.
Locked or constrained affected points reject the complete operation. Creation
rejects crossing/degenerate faces and more than two faces on an edge. A no-op
does not create history or consume a revision. Any geometry/output failure uses
the same full model/history rollback as coordinate edits.

## Advanced edits

`method=advanced` requires the current document/revision and nonempty, unique
typed `ids`, with `kind=point|edge|face|curve`. Shared control points are changed
once. Any locked or constraint-related point rejects the entire selection.

- `translate|scale|rotate`: three finite `vector` values, optional Boolean
  `adjust_markers` (true only for an all-control-point selection). Positive scale
  factors; rotations in degrees, original RotateVector convention, about origin.
  Whole-model scale uses Model_Scale to preserve related settings. Whole-model
  translation shifts explicit split position and all four section plane offsets.
- `extrude`: kind=edge, three-element `vector`, live `layer_id`. Only boundary or
  free-standing edges; shared endpoints create one shared new vertex. Reject
  interior edges, center-plane clamping and degenerate sides. New faces oppose
  their old neighbor's direction on the shared edge.
- `mirror`: kind=face, integer `axis` 0..2, finite `distance`, Boolean `weld`.
  Reflect in x/y/z=distance, reverse face order, preserve layer/edge crease. Fully
  coplanar faces are unchanged. Welding uses the original positional tolerance;
  duplicate faces, repeated vertices, non-manifold edges or locked weld targets
  reject the transaction. Layer symmetry is preserved.

No-ops preserve revision and redo. All edits use the existing transactional
rebuild/export/history rollback and refresh control-curve and flowline caches.
FBM remains the original serialization; camera operations do not send edits.

## Section and layer management details

`method=sections` uses the same document/revision guard. `kind` is 1 station,
2 buttock, 3 waterline, 4 diagonal. `operation` is:

- `add`: `positions` contains 1..512 finite distances, in current document units.
  The stored Single domain decides duplicate distances; duplicates are no-ops.
  A batch may not take the total past 4096 definitions. Sort once per batch.
- `update`: zero-based kind-local `index`, optional `distance`, `show_curvature`.
  Indices are scoped to this exact revision, not persistent identities. Resort
  after an edit. The Qt table clears selection when a new revision arrives.
- `delete`: nonempty unique zero-based `indices` for this kind.
- `clear`: remove all definitions of this kind. An empty list is a no-op.

Station/buttock/waterline distances are the respective axis coordinates. For a
new diagonal the inherited 45-degree plane is y+z=distance; existing diagonal
normals are retained when changing distance. Definitions outside the hull are
retained and saved even when their curve is empty (a deliberate change from the
old Add dialog). Adding diagonals and setting a curvature flag promote old FBM
versions only when necessary to persist these fields. Curve-comb display remains
unported; the GUI shows the retained flag read-only.

The model JSON now contains `sections:[{kind,index,distance,show_curvature,color,
plane:[a,b,c,d]}]`. The C++ worker emits `FreeShipSectionDisplay` version 1 with
matching kind/index/plane and world-coordinate `paths`. These reuse completed
cuts; sampling a copy at 256/512/1024/2048 intervals plus all knuckles cannot
change the lines-plan or DXF. Qt validates the hash, all keys/planes and finite
point limits before publishing the immutable snapshot. Camera changes only draw
these coordinates. All three cached artifacts are hash-checked; corrupt section
data triggers regeneration. Section edits invalidate intersections without
repeating the unchanged Pascal hull subdivision.

`method=manage_layers`, `operation=create` takes a `changes` patch using the
existing `update_layer` schema. `delete` takes a stable `layer_id` and rejects
occupied layers and deletion of the last layer. `delete_empty` removes only
empty layers, keeping at least one. Deletion, restoration and branching retain
the same non-reused ID rules as topology edits.

## Optional four-view presentation metadata

Model snapshots may include `overlay`. Older frozen fixtures without it remain
readable and omit these annotations. It contains Boolean `show_grid`,
`show_hydrostatics`, `main_particulars`; 24-bit Pascal BGR `grid_color`,
`grid_font_color`, `hydro_color`; integer `point_size`, `font_size` in 1..100;
and `hydro` with `available`, `markers`, `areas`, `path`.

Markers contain unique `key`, formatted `text`, and finite world position `p`.
Area entries contain `p` and `value`; the area-curve path is sampled using the
original hydrostatic spline at i/600, including both endpoints. An unavailable
calculation has no result arrays. Snapshot validation bounds marker/area/path
counts to 32/10000/100000 and rejects nonfinite coordinates or invalid metadata.
Control points additionally carry `name`, `vertex_type` (0..3), and `leak`.

These fields are read-only presentation data. The bridge owns a separate lazy
hydrostatic calculation for overlays; it must not change the shared design
calculation's draft, which also controls the legacy crease split. Successful
model edits explicitly invalidate analysis caches; display switches and camera
changes do not. Qt consumes the same immutable model revision and never invokes
hydrostatic calculation from a paint callback. Presentation animation and layout
cannot alter the complete lines-plan scene, DXF, or saved model. Image export
draws the final presentation independently of the current animation time.

## Modeling completion commands

All operations below use the existing document/revision guard, atomic publish,
undo/redo and FBM persistence. `method=modeling` carries `operation`:

- `anchor`: ordered point `ids` (first is the reference), Boolean `hard`.
- `align` / `linear_constraint`: first and last point IDs define the line; the
  intermediate points are projected. The latter retains references. Cycles are
  rejected before updating coordinates. `coincide` moves the remaining points to
  the first point, retaining their topology. `clear_constraints` removes the
  selected points' references; locked points must be unlocked first.
- `point_properties`: one point ID, `name`, `vertex_type` in 0..3. Names must be
  representable in the active FBM encoding; FBM has no per-file encoding tag.
- `unlock_all`, `split_edges`, `collapse_points`, `collapse_edges`,
  `connect_points`, `extrude_points`, `remove_unused`, `flip_faces`,
  `delete_negative`. Selections use typed stable IDs. Extrusion takes a `position`
  offset and creates points only. Edge collapse merges faces. Point collapse
  requires degree two and rejects separate curve junctions. Face diagonals must
  remain inside the face's dominant projection. Negative-side deletion does not
  clip crossing faces; it deletes wholly nonpositive-Y faces and unused negative
  points, following the recorded Plus tolerances.
- `create_group` / `update_group`: point `ids`, `name`, Boolean `locked`; update
  also takes `index`. `delete_group` takes the revision-scoped group `index`.
  FBM does not persist Group.Locked, so group locking uses persisted point locks.
  Creating an unlocked group keeps existing point locks; explicit group unlock
  clears them. Dissolving a group retains its points and their locks.
- `insert_plane`: `axis` 0..2, three-component `position`, `create_curve`.
  `intersect_layers`: target `layer_id`, cutter `other_layer_id`; inserts target
  control-edge intersections with the cutter's subdivided surface.
- `grid_panel`: `origin`, positive `size_a` / `size_b`, `columns` / `rows` 2..100,
  `plane` 0=XY / 1=XZ / 2=YZ, `layer_id`. Dimensions span intervals, including both
  endpoints. `cylinder`: `origin`, `end`, positive `radius`, `segments` 4..128,
  `close_start`, `close_end`, `layer_id`; uses the original interpolating ring.
- `auto_group_layers`: optional selected face `ids`, otherwise visible faces.
  `assemble_faces`: visible source layers, handled separately so existing face
  material/calculation attributes survive. `repair_normals`: consistent adjacent
  winding; refuses nonmanifold/nonorientable components and constrained edits.
- `add_markers`: `paths` of finite XYZ arrays (2+ points per path, at most 4096
  paths and 100000 new points); `delete_markers` removes all markers.
- `lackenby`: selected layer `ids`, target `displacement` in current project
  displacement units, world-coordinate `lcb`, `iterations` 1..100. Nonconvergence
  discards the complete operation. `inspect_modeling` is read-only and returns
  volume/displacement/LCB/Cp/length/midship without adding history.

The four construction/intersection commands now report `edit_engine=cpp-topology`
and use the native control-net writer. `lackenby` reports `cpp-lackenby`,
`cpp-coordinate-writer` and `lackenby_edit_ms`; `inspect_modeling` reports
`analysis_engine=cpp-lackenby` and runs without the Pascal measurement callback.
The external-document adapter rejects these old edit/measurement entry points.
Plane cut ordering is deterministic; see `NATIVE-CONSTRUCTION.md` for the exact
legacy heap-order reference variants and the near-endpoint null-result fix.

`method=new` takes a fresh `document_id`, positive `length` / `beam` / `draft`,
`columns` / `rows` 2..100, `units` 0=metric / 1=imperial. If replacing a document,
the old document/revision guard is still required. A candidate is published only
after construction and export succeed. A new file starts modified and has no
source thumbnail. It defaults to `SavePreview=True`, with a valid blank JPEG
until GUI save supplies the current four-view rendering. Loaded files keep their
own flag. `update_project.changes.save_preview` accepts a Boolean, removes the
JPEG when false, and initializes a valid blank image when enabling a disabled
preview. Explicit enabling promotes pre-2.1 formats; passive saves do not.
Published `settings.save_preview` exposes the persisted flag to Project Settings.

`advanced/rotate` optionally accepts a three-component `pivot`; absent means the
original origin. The Qt midship action supplies `[split_section_location,0,0]`.
Display accepts `show_normals` and `show_markers`. Auxiliary `rename_control`
accepts one curve ID and `name`; branched selected edges create separate curves
ending at branch vertices rather than guessing a traversal through a junction.

Optional snapshot fields: point `anchor`, `line_a`, `line_b`, `hard_anchor`;
`point_groups:[{index,name,locked,points}]`; `markers:[{index,visible,color,spline}]`;
`surface_normals:[{face,p,n}]`, `normal_color`. Normals originate from subdivided
vertices, are drawn only for selected faces, and their traversal order is not a
persistence guarantee. All reference targets, finite values and count limits are
validated. None of these display overlays alters native lines-plan/DXF geometry.

### Hydrostatics and stability

`hydro_defaults` is read-only and returns `analysis` with stored table ranges,
angles/displacements, VCG, free-trim and trim settings, datum and coefficient basis.
`hydrostatics` takes `output`, `mode`, `trim` and the current document/revision guard:

- `single`: `draft >= 0`, `heel` in [-85,85].
- `table`: `start`, `end`, `step >= 1e-6`, at most 200 rows; heel is zero.
- `cross`: strictly increasing `angles` [-180,180] and positive `displacements`,
  at most 181 angles, 100 masses and 500 conditions; required `lcg/tcg/vcg` in
  model coordinates and Boolean `free_trim`. Length/mass units follow the model.

The read-only result is `hydro.json`, with `schema`, `mode`, `inputs`, units,
water density, appendage coefficient, keel datum, document/revision and `rows`.
Rows are `status=ok` or `status=error` with a message; failed quantities are absent,
never fake zeros. Asymmetric static sections/area-derived quantities are explicitly
null with `partial=true` and an explanation. Cross rows include target and actual
mass, KN/GZ, solver residuals/tolerances, equilibrium plane and projected gravity.
GZ area is trapezoidal in radians, starting at zero heel; errors break the integral.
`output/cancel.flag` cancels a job. Early cancellation may be a normal error response;
in-progress cancellation returns `cancelled=true`. Callers retain the previous
complete result and discard partial cancelled rows. No calculation creates history.

`hydro_settings` stores table ranges or cross angle/mass lists, trim, VCG and
free-trim through the normal transaction. No-op does not promote the file version
or discard redo. FBM versions below 2.5 are promoted only when values change;
unrepresentable distinct Single list entries are rejected. LCG/TCG stay report-only.
Report JSON preserves original floating values; display rounding is separate.

### Resistance and propulsion

For `propeller1` through `propeller5`, `performance_defaults` and `performance`
execute in the native document owner and return `analysis_engine=cpp-propulsion`.
The result artifact retains the existing schema and series contract. Original
external solvers remain dependencies; no Pascal adapter participates in these
requests. The adapter rejects these operations when `external_document=true`.
Resistance and matched-propeller caches require the same document and revision.
The ten resistance methods also execute in C++, returning `analysis_engine=cpp-resistance`.
Their original external solvers remain separate dependencies. Hull extraction uses
native hydrostatics. A failed recomputation invalidates the previous resistance
generation; only complete successful outputs are published. Optional `model_name`
provides the report filename. The backend supplies the document text encoding.

`performance_defaults`, `performance`, and `performance_settings` require the
current document/revision guard, `output`, and `calculator` (delft, holtrop,
hollenbach, oortmerssen, fung, ost, rbhs, mh, kaper, planing, propeller1..5).
Optional `values` is a strictly typed field-key object from the returned schema.
Unknown fields, nonfinite numbers, out-of-range choices and stale state are rejected.
Dependent choices are applied before scalar values, independent of JSON key order.

Defaults return `performance.json` containing title, field labels/units/groups,
types, ranges, choices, values, enabled/visible states and source label identifiers.
`extract:true` explicitly extracts hull data or resistance from the same document
revision. `solver_directory` optionally selects existing external tools; it is
not a request to download/install them. All except Delft/KAPER require metric units.

`performance` runs on a disposable model and returns series with original X/Y,
per-curve axis labels, original chart labels, text/help/warnings, actual parameters,
requested inputs, elapsed time and document identity. Scaled original ordinates
(e.g. 10 times resistance) remain scaled and are labeled accordingly. No successful
result is returned for missing/invalid solver output. `output/cancel.flag` aborts
the operation; the UI retains its previous complete result. Private solver files
are bounded diagnostic evidence, not model content or an engineering certificate.

`performance_settings` persists the legacy FBM subset through a normal transaction.
Full cases are separate `FreeShipPerformanceParameters` JSON files, version 1,
with calculator, units and values; solver paths are omitted. Resistance exchange
is private to one Qt session and keyed by document ID/revision. This is distinct
from the cross-restart lines-plan cache.

## Developed plates

`develop_plates` requires the current document ID/revision and a private `output`.
It unfolds developable layers on a disposable document and returns `plates.json`
(schema 1), including the document identity, units, design waterline/colors,
points, original 3D coordinates, indexed edges/faces, boundary flags, error values
and four types of section curves. Connected symmetric halves are explicit data.
`output/cancel.flag` aborts cooperatively; no partial result is published and no
model history is changed. Qt validates indices, finite values and closed boundary
graphs before replacing the previous complete scene. Layout is local to the
window; export always uses complete visible geometry, not the camera rectangle.

## Model interchange

`import_model` takes `path` (a private UTF-8 `FreeShipExchange` version 1 JSON),
a new `document_id` and `output`. With an existing document it also requires
`expected_document_id` / `expected_revision`. C++ parses OBJ/STL/FEF before this
request. The payload contains `project`, `creator`, `particulars_set`, `points`
(`p`, `vertex_type`), `edges` (`[a,b,crease]`), `faces` (`points`, `layer`) and
`layers` (same editable properties as a layer update). References are zero based.

The service revalidates bounds/types, references and connectivity, builds an
isolated candidate and generates all artifacts before adoption. Success starts a
new unsaved document at revision 1; it does not append a destructive replacement
to the previous document's undo history. Failure preserves that history. Imported
documents have no FBM source path until saved. Model snapshots additionally expose
`settings.created_by` and `settings.particulars_set` for the FEF metadata subset.

OBJ/STL/FEF exports run in C++ against a complete immutable snapshot, with no
backend edit command. FEF stores all original control geometry; mesh exports use
explicit scope/units/axes instead of camera state. See `MODEL-EXCHANGE.md` for the
format-specific information that is not representable in these files.

## CAD surface data

`cad_iges` requires the current document ID/revision and a private `output`.
The service converts a disposable history snapshot with the existing NURBS
algorithm and returns `cad.json`: schema 1, document identity/revision and
`patches` containing stable layer IDs, rows/columns, degrees, knot vectors and
row-major control points (column varies fastest). Patches cover original-side
layers; Qt applies explicit visibility/mirroring and unit conversion on writing.
The editing document is never rebuilt in a different subdivision mode.
`output/cancel.flag` cooperatively cancels between patches; time/size limits fail
the request. No model revision, undo entry or dirty state changes on success.
Qt checks document/version, finite values, node counts/order, control-net size
and layer identity before committing the target file.

## GHS 独立导出进程

`FreeShipGeometryExport.exe request.json report.json [parent_pid]` 读取当前已发布的
`model.json` 与同目录 `native-input.json`。请求包含 `model`、`input`、`output`、
`document_id`、`revision`、`units`（m/ft）和布尔 `close_deck`。
两个输入与请求的文档身份、版本及单位必须一致。正常 UI 只让子进程写私有
临时文件；报告返回 SHA-256、站数/点数、封顶站位、空站位和中心线分段站位。
主进程再次验证当前文档和哈希，用原子复制提交用户所选路径。
没有新增 Pascal 编辑命令，不改变脏标记、撤销栈或快照。关闭取消只终止本次
拥有的导出进程；超时 90 秒，父进程退出也终止。

## Archimedes / PAM 原生输出

同一个导出进程新增 `format` 字段：`ghs`（省略时的兼容默认值）、
`arch_hll`、`arch_app`、`pam`。新增三种格式的 `units` 必须为 `m`。
可选 `references` 对象使用米制有限数值：Archimedes 的 `ap` / `fp`，
HLL 另用 `aft_mark` / `mid_mark` / `fwd_mark`；PAM 用 `midship` / `waterline`。
未提供时按导出窗口的同一默认规则初始化。新 UI 总是显式传入窗口里的值。

报告补充 `format`、`references_m` 和 `half_section_points`；`points` 为实际
文件中写出的点数。HLL/APP 镜像为完整轮廓，PAM 保留半船。没有新增后端编辑命令，
不会改变原模型或保存工程中的参考设置。PAM 仍是旧格式模板，不是求解器请求。

## Native document ownership

`update_project.changes.created_by` edits the existing encoded FBM text field.
`align_to_baseline:true` is an explicit one-shot edit, not a persistent automatic
calculation flag. It follows unit conversion, translates the entire model,
markers, section planes and projected flow seeds, and rejects locked/constrained
points atomically with other changes. A zero-keel no-op does not create history.

`performance_defaults` / `performance` for `propeller4` now expose
`values.WaterViscosity` (0.01–100, in 10^-6 m^2/s, default 1.189 as Single).
Its full parameter JSON preserves the input; FBM has no corresponding field.
The `performance_settings` validator accepts it but does not claim it is stored.
Maneuver extraction retains the existing `Edit11` viscosity instead of resetting
it from an unstored temperature. Both Qt dialogs offer bounded temperature-table
conversion, with no geometry or document mutation.

The existing public methods and revision guards remain compatible. Responses
identify `document_engine=cpp-document` and expose document timing plus the owned
`adapter_pid` for diagnostics. `document_status` is read-only and returns the
structural FBM inspection and current persistent-byte SHA-256. User saves do not
call the Pascal save method.

Internal adapter requests add `external_document=true`. `checkpoint=true` returns
`state_file` plus `binding_file` containing ordered IDs and per-kind allocation
high-water marks. The adapter releases prior history states after each operation.
`restore_document` receives FBM, bindings, document ID/revision and `publish`; it
loads a working copy without inventing new entity identities. These are internal
interfaces and not editable document commands exposed by the native service.

The owner validates all artifacts before committing. A failed/crashed adapter is
discarded; later operations rehydrate from the committed checkpoint. History and
the clean token remain unchanged. The original loaded bytes and canonical adapter
checkpoint are distinct because Pascal SaveProject can reorder control points.

Native basic control-net edits identify `edit_engine=cpp-topology`,
`fbm_write_engine=cpp-control-net-writer`, `topology_edit_ms` and their publication
operation. These timings separate candidate work from full document rebuilding.
The adapter rejects delegated basic topology, the fourteen migrated modeling
commands and every `advanced` command in external ownership mode; the direct
oracle mode remains available. Translation, scaling and rotation identify
`edit_engine=cpp-transforms`, `fbm_write_engine=cpp-transform-writer` and
`transform_edit_ms`. Extrusion and reflection use the native topology writer.

Bindings may include `point_edges`: one ordered incident stable-edge-ID array per
serialized point. This preserves live split/collapse identity behavior across
native publication and worker recovery. Every incident edge must appear exactly
once and belong to that point. Undo/redo deliberately omit it, matching old FBM
reload ordering; it is not persisted in the FBM format.

`point_anchors` and `point_lines` have the same per-serialized-point shape but
contain ordered dependent point IDs. They preserve the existing setter's live
reverse-list order; otherwise a diamond of line/anchor dependencies can move to
a different coordinate after the adapter reloads a native candidate. Both the
C++ editor and adapter validate the membership, count and uniqueness. Checkpoints
capture actual live lists, recovery replays them, and undo/redo reconstruct file
order like the original implementation. They are not FBM extensions.

Format promotion now runs inside the native document service, to the minimum
version required by the requested field. The converted bytes and edited candidate
commit together after publication checks. Unchanged commands do not create a
conversion history step. The old `promote_document` remains only for direct
oracle use; `external_document=true` rejects it.

`update_project` returns `edit_engine=cpp-project`, `project_edit_ms` and
`fbm_write_engine=cpp-project-writer`. `update_layer`, `manage_layers` and
`sections` return `cpp-definitions`, `definition_edit_ms` and
`cpp-definition-writer`. Public requests, revision guards and artifacts are
unchanged. `restore_document` publishes native candidates; `snapshot` handles
unchanged requests. The adapter rejects external execution of these old edits.

Bindings carry `fbm_encoding`, `layer_name_prefix`, `default_layer_color`,
`default_underwater_color` and `resolved_colors` keyed by signed raw TColor.
They describe preferences/palette, not FBM extensions, so equivalent submissions
preserve default labels and system colors. Duplicate request keys are rejected
before QJsonObject can discard the first value.

`background` returns `edit_engine=cpp-background`, `background_edit_ms` and
`fbm_write_engine=cpp-background-writer`. `display` and `auxiliary` return
`cpp-auxiliary`, `auxiliary_edit_ms` and `cpp-auxiliary-writer`. Their old editor
implementations reject `external_document=true`; direct oracle requests remain
available. Unchanged requests keep history/revision; changed candidates publish
through `restore_document`. Native control-curve IDs and ordered controls are
checked against the published snapshot. Newly created flowlines require at least
two computed spline points before the native transaction can commit. The tracing
algorithm remains a read-only adapter calculation, not a native field edit.

The document owner forces `native_surface=true` alongside `native_linesplan`.
Only direct oracle requests can use the old snapshot generator. Native input
adds `surface_display` with stable `faces/layers/curves` ID arrays in geometry
order and `show_normals`. Cardinality, integer ranges and uniqueness are checked.
The same subdivision level produces `surface.json` (`FreeShipSurfaceDisplay`,
version 1), carrying triangles, per-root point normals and control-curve splines.
Scene and DXF generation is unchanged. The native report includes
`surface_display`, `surface_display_ms` and `surface_sha256`; successful snapshot
responses add `surface`. Both document service and Qt session require matching
hashes and exact agreement with the snapshot geometry before adopting a revision.

The cache key includes stable IDs and display flags, but excludes document UUID
and revision. Its report binds cached contents to the current request identity.
Corrupt or missing surface artifacts regenerate the whole cache entry. Cache
eviction includes `surface.json` in its existing 16-entry / 256 MiB budget.
Snapshot diagnostics expose `surface_engine=cpp-native` and
`legacy_triangle_count=0`; the direct old oracle reports `pascal-oracle`.

Fresh `load`, `new`, and `import_model` requests return
`creation_engine=cpp-document-create`, `creation_ms`, and
`adapter_operation=restore_document`. C++ validates/constructs the FBM and stable
bindings, then publishes an analysis snapshot; the old external operations reject
these creation methods. Only successful publication adopts the new UUID/history.

Internal `document_context` supplies encoding, colors, localized default layer
prefix and the history limit. Initial layer/underwater colors are separate from
loaded preference defaults to match the original constructor's initialization
order. This command creates no active document. Format ordinals 0..6 are upgraded
to 7 only for transport to the analysis decoder; original save/history bytes
retain the actual format until an edit requires a persisted field upgrade.

`develop_plates` runs in the C++ document service and returns `plates` plus
`analysis_engine=cpp-plates`. The schema-1 JSON keeps its existing geometry,
metrics and styles, and adds `analysis_engine` and `calculation_ms`. Identity
and revision are checked before calculation. A completed report is published
atomically; cancellation and failure leave the document and history untouched.
`document_context.plate_context` provides localized part text, the inherited DXF
palette and four section colors. It supplies no calculated plate geometry.
The external-document Pascal `develop_plates` route rejects production calls.
