# Auto Decor 0.7 — Geode / Geometry Dash 2.2081 (Windows)

A major change from v0.6: 4 distinct styles, not repeated full-sized blocks. Generates in ~18 objects per frame and displays a progress indicator.

**NEON:** layered thin glow, soft glow borders, large focal rings and small internal circuitry.
**TECH:** mechanical panel texture, square/hex conduits, bolts, nested gear mechanisms and energy nodes.
**RUINS:** masonry seams, mixed bricks, chipped accents, stone monuments and hanging chains.
**NATURE:** upper-surface vegetation, internal rocky ground, flowers, suspended clouds and ambient fireflies.

Each style includes four color harmonies. The NEW COLORS button cycles the selection. Each generation allocates fresh three-channel base/trim/highlight colors, which are stored with the level.

Whole level / selected target and Clean/Normal/Rich modes remain. Decoration is scheduled in small batches, capped at 24,000 pieces. Collision solids, spikes and gameplay modifiers are explicitly rejected upon creation. Back up levels: undo tracking is session-local.

**Limitations:** Visual aesthetic is unverified until an actual GD editor test. Smart tiling of slopes or complex gameplay structures is still approximated by contextual decorations, not hand-built art. Color changes and object layering should be tested on a copied level. Does not use neural networks or copyrighted external textures.

Source and GitHub Actions build in this branch: feature/gd-autodecor-build.
