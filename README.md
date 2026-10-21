# Pixelle

A raster image editor for Linux, written in C++17 with Qt 6 Widgets (QImage/QPainter raster engine).

> **Interface status:** the menus, tool bar, option bar, palette and status bar are a **functional
> placeholder layout**. They are *not* a reproduction of the Windows 11 Paint ribbon/title bar, and
> no pixel-fidelity claim is made. That work is blocked on reference screenshots and icon assets
> (see `docs/MISSING_REFERENCES.md`). Every unmeasured dimension is tagged UNVERIFIED in
> `src/ui/StyleTokens.h`.

## Build (Ubuntu 24.04)
    sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools \
        qt6-image-formats-plugins libgl1-mesa-dev libxkbcommon-dev
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

## Run
    ./build/pixelle [image-file]

## Test
    QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

## Screenshot comparison (python3, Pillow, numpy, scipy)
    python3 tools/compare.py reference/<ref>.png current.png reports/<name>
Refuses to compare images of different size; writes overlay, amplified diff, JSON metrics and region boxes.

## Architecture
    src/drawing/  DrawingDocument (QImage + dirty state)   Raster (pure pixel ops)   ImageOps (whole-image ops)
                  Editor (settings, tools, selection, clipboard, history glue)   Tools.cpp (pointer -> engine)
    src/history/  UndoStack: tile-based region transactions, full-image entries, saved-state tracking
    src/io/       ImageFileService: load/save, atomic writes, overwrite guard, alpha handling
    src/ui/       CanvasView (zoom, coordinate mapping, input, overlay)  ColorPalette  Dialogs  StyleTokens
    src/app/      MainWindow (menus, shortcuts, file workflow), main
Tools receive IMAGE coordinates only; `CanvasView` converts viewport coordinates, so zoom cannot leak into editing.

## Features (all covered by automated tests unless noted)
| Area | Status |
|---|---|
| Pencil (size, aliased), 6 brush types, eraser (bg colour or transparent) | working |
| Fill bucket (4-connected scanline, tolerance), eyedropper (left=fg, right=bg) | working |
| Line, rectangle, rounded rectangle, ellipse, triangle, diamond, pentagon, hexagon, star; outline/fill/both; Shift constrain | working |
| Text (dialog -> movable floating text -> commit) | working (not in-canvas editing) |
| Foreground/background colours, swap (X), size, antialias toggle, palette + colour dialog | working |
| Rectangular & free-form selection, move, Ctrl+drag copy, delete, cut/copy/paste, select all | working |
| Crop to selection, resize image, canvas size, rotate 90/180, flip H/V | working |
| Undo/redo for every document-changing operation; tile-sized stroke undo; saved-state dirty tracking | working |
| New / Open / Save / Save As, unsaved-changes prompt, overwrite guard, transparency warnings | working |
| Zoom 1/16x-16x (Ctrl+= / Ctrl+- / Ctrl+0 / Ctrl+wheel), middle-drag pan | working |
| Shortcuts: Ctrl+N/O/S/Shift+S/Q, Z/Y/Shift+Z, X/C/V, A, Del, W, E, Shift+X, ], [, Esc, Enter, X | working |

## Known limitations / outstanding
* **Windows 11 Paint fidelity: not started** (no references). No title bar, ribbon, icons or measured geometry.
* Selection resize handles, selection rotate/flip, skew, polygon and curve tools, and in-canvas text editing are not implemented.
* Image rotate/flip/crop apply to the whole image (a pending selection is committed first).
* Whole-image operations (resize, rotate, canvas size) store full before/after images in history (stroke-level edits do not).
* Qt's aliased (non-antialiased) ellipse rasteriser is mirror-symmetric only to +/-1 px; antialiasing is on by default.
* Brush types are functional approximations; their authentic Paint appearance is unverified.
* Undoing an uncommitted move/paste discards the pending selection (document untouched) rather than restoring the old selection.
* Pasting an image larger than the canvas enlarges the canvas as a separate undo step.
* Palette colours and layout are placeholders.
* Packaging: `.desktop` file and install rules exist; no icon (none authored without reference).
