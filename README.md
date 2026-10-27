# Auto Decor 0.5.0 — fix for 5-block limit

**Geometry Dash 2.2081 / Geode 5.11.2 / Windows Steam**

## What changed
- **TARGET: ALL BLOCKS** (now default) processes all suitable full-size solid grid blocks across the level.
- **TARGET: SELECTED** processes only a selection, including selected decorations.
- Up to 20,000 source blocks and 24,000 generated pieces per run, with batch generation of 85 objects per frame.
- No more hardcoded 700-selection cap. Clear diagnostic report: scanned objects, accepted blocks, placed pieces, rejected objects.
- Keeps four styles (Neon, Tech, Ruins, Nature), clean/normal/rich detail modes, 10 undo batches.

## Test
Back up the level. Remove old craftkot.autodecor.geode. Install updated file into Geometry Dash/geode/mods.
Open editor > AUTO DECO > TARGET: ALL BLOCKS > any style.
Test on a simple level with 15-50 ordinary blocks. Read the resulting dialog for actual input and generated counts.

IMPORTANT: Modern Geometry Dash objects come from many object classes. The whole-level mode only modifies **ordinary 30px unrotated solid or breakable grid tiles** for safety; this is not a full AI art-generation system. Blocks outside the relevant scope or not aligned to a 30px lattice aren't processed. Large levels might need multiple passes. Do not use AUTO ALL on levels with existing decorations without backing up: overlaid tiles may obscure other objects.
