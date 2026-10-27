# Auto Decor v0.4.0 — Geometry Dash 2.2081, Windows

Experimental Geode add-on for the GD editor. Back up the level before testing.

## How to use

1. In the editor select ordinary 30x30 solid blocks (max 700).
2. Press AUTO DECO (top left).
3. Choose DETAIL: CLEAN, NORMAL or RICH (Normal by default).
4. Click NEON, TECH, RUINS or NATURE.
5. Playtest the result. Press UNDO DECO to remove the latest batch; this history tracks up to 8 batches within the current editor session.

## Improved generator

Each theme now uses its own verified cohesive object family and shared 8-neighbor autotiling for connected block silhouettes:

- NEON: neon block family 1162..1186, accent outlines 1191..
- TECH: mechanical 263..268, geometric conduits 237..241.
- RUINS: colored brick family 269..275, sparse aged brick details 277,278,245.
- NATURE: two-color grass 952..959, terrain details 1048, vegetation 406..408 and 939..942.

All decorative objects are placed on or near the selected blocks. Max 1750 pieces in a batch; selections must be grid aligned. The generator does not decorate slopes or guarantee professional art quality. It is procedural, not ML.

## Technical notes

GD 2.2081 + Geode 5.11.2 on Windows only. GD 2.208 support unverified.

Auto Decor objects are placed on visual layers above the selected blocks. They may change visibility or collisions; always playtest and use a copy of the level. Batch Undo only removes objects still found by their unique IDs. Each action also creates native editor undo records.

The source is in src/main.cpp. CI in .github/workflows/build-auto-decor.yml. Reference object catalog: https://flowvix.github.io/gd-info-explorer/ids
