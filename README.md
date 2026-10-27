# Auto Decor 0.6.0 — Geometry Dash 2.2081, Windows

## Why it previously decorated only a handful of objects
Auto Decor v0.5.0 classified only full, unrotated Solid/Breakable tiles on a lattice locked to the *first* source object. All the rest (slopes, hazards, special objects and off-grid geometry) were silently ignored. Auto-tiling also used opaque sprites without assigning the styles' color channels, producing generic white slabs.

## Changes in 0.6
- Dominant grid-origin detection for solid tiles; alternate-offset solids become scenery anchors instead of vanishing.
- Gameplay anchors: slopes, decorative shapes, portals, hazards and special objects receive limited BACKGROUND supports instead of dangerous overlapping opaque tile overlays.
- Three own RGB color channels per theme; assignments are written to the level using Geode's color effect manager.
- Main tiles placed behind original solid blocks (B1), with accents in foreground; no gameplay object intentionally replaced.
- Kept modes TARGET ALL / SELECTED and clean/normal/rich levels.
- Improved debug report showing source blocks, context anchors, generated counts.
- Generation 85 objects per frame to avoid large editor freezes.

NOTE: non-square shapes are only given limited supporting background decor; they are *not* fully auto-tiled. This is an experimental geometry-aware procedural tool, not a professional ML decorator.

## TEST SAFELY
Back up your level. Close Geometry Dash, replace `craftkot.autodecor.geode` inside `Geometry Dash/geode/mods`, reopen the editor, choose TARGET ALL, then NEON / NORMAL. Try a simple level with 15+ regular blocks, slopes and an orb/portal. Compare result with 0.5.0. The in-game rendering has NOT been verified automatically. Playtest and use Undo Deco if needed.

Target: Steam GD 2.2081, Geode 5.11.2, Windows.
