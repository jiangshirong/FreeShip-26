# FreeShip Qt migration

The user authorized the main agent to plan the migration, delegate to subagents,
integrate their changes and validate the results in one main chat.

Conserve the user's quota: default to one agent. Delegate only independent major
workstreams, not routine implementation/test/review slices. Reuse verified test
infrastructure and run checks appropriate to the change.

The user corrected storage on 2026-11-03 and requested another cleanup on
2026-11-04: choose C or D according to current free capacity, not a permanent
drive preference. The unified test runner selects the roomier configured root
when --output is omitted and retires only its own completed run's transient
lines-plan caches. Reuse the current compilation directory; do not create a
fresh full build for each test. Keep reusable test source in ../FreeShip-Tests,
reports and required baselines. Preserve historical evidence paths and links.

- Preserve ../FreeShip-Plus-Lazarus and ../FreeShip-QtPilot as baselines. Do not
  change their source, installed binaries, user models, preferences, or fs entry.
- Assign disjoint files or private worktrees to writers. Coordinate interface
  changes with the main agent. Use private build/output directories.
- Model state belongs to one backend. UI snapshots are immutable. Every editing
  request includes the expected document ID and revision; reject stale requests.
- Keep the accepted lines-plan renderer and numerical/export behavior. Do not
  simplify curves, reduce accuracy, or change expectations to pass tests.
- UI migration must preserve the user's existing StudioLight design, original
  icons, terminology, layout and interactions. The Qt prototype's temporary
  controls are not the visual baseline. Track parity separately in
  docs/migration/UI-FIDELITY.md; do not replace the design or mark unported
  overlays, animations or dialogs complete because the main shell is present.
- Integrate reusable tests into ../FreeShip-Tests and its unified entry. Report
  pass/fail/review/not_run/error separately. Hidden Qt object/event tests do not
  replace actual desktop acceptance. Do not use computer-use for acceptance.
- Keep source licenses and provenance. Record implementation and pending scope in
  docs/migration. Do not claim the whole migration is complete at a milestone.
- ShipCAD is an optional reference, not a required implementation or correctness
  oracle. Choose reuse, adaptation or a new implementation per module. Check
  current Plus compatibility and independent mathematical expectations separately.

