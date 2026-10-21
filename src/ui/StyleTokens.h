#pragma once
// Single source of truth for interface geometry and colors.
//
// Product: Pixelle. NOTE: the interface chrome is a FUNCTIONAL PLACEHOLDER; it is not
// (yet) a reproduction of Windows 11 Paint.
//
// STATUS: every value marked UNVERIFIED was NOT measured from a reference
// screenshot (none exist in /reference yet). They are placeholders so the
// layout compiles; they MUST be replaced by measured values before any
// fidelity claim is made. Verified values will be tagged MEASURED with the
// reference filename they came from.
#include <QColor>

namespace PaintStyle {

// --- Reference environment (to be filled when references are supplied) ---
// Paint build: UNKNOWN   Window size: UNKNOWN   Scale: UNKNOWN

// --- Workspace / canvas (UNVERIFIED) ---
constexpr int kCanvasOffsetX = 8;
constexpr int kCanvasOffsetY = 8;
inline QColor workspaceBackground() { return QColor(0xE6, 0xE6, 0xE6); } // UNVERIFIED
inline QColor canvasBorder()        { return QColor(0xA0, 0xA0, 0xA0); } // UNVERIFIED

// --- Chrome placeholders (UNVERIFIED) ---
constexpr int kRibbonHeight   = 0; // not implemented until references exist
constexpr int kStatusBarHeight = 0;


// --- Colour palette widget (UNVERIFIED: functional placeholder, not measured from Paint) ---
constexpr int kSwatchSize = 22;
constexpr int kSwatchGap = 3;
constexpr int kIndicatorSize = 30;

// --- Zoom steps (functional; Paint's exact steps UNVERIFIED) ---
inline const double *zoomSteps(int *n) {
    static const double z[] = {0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0};
    *n = int(sizeof(z) / sizeof(z[0]));
    return z;
}

} // namespace PaintStyle
