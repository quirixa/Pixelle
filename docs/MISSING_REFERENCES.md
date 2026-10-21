# Missing references (blocking Windows 11 Paint visual fidelity for Pixelle)

No authentic Windows 11 Paint screenshots or icon assets exist in this environment
(`/mnt/user-data/uploads` was empty; filesystem search found none). Per project rules
1, 2, 4 and 6, the ribbon/title bar/status bar are NOT implemented and no fidelity
claim is made. All geometry in `src/ui/StyleTokens.h` is tagged UNVERIFIED.

## Please provide (place in /reference with descriptive names)
1. Default window, full-window PNG, at **100% display scale**, window size noted.
2. Each ribbon tab (Home, View) plus expanded dropdowns (brushes, shapes, size, colour editor).
3. Paint build number (Settings > Apps > Installed apps > Paint) and Windows build.
4. Display resolution + scale factor; title bar behaviour (Mica? caption buttons?).
5. Icon assets (SVG preferred, or PNG at 1x/2x/3x) - or confirm I should redraw them
   from the screenshots (which cannot be pixel-exact).
6. Status bar and zoom slider close-ups; selection handles; dialogs (resize, save).
7. Segoe UI Variable is not available on Ubuntu. Supply the font file if licensing
   allows, otherwise font-rendering differences are a documented, unavoidable diff.

## Known differences expected regardless
- Window frame/title bar: drawn by the Linux window manager unless a frameless custom
  title bar is built from references.
- Font rasterisation (ClearType vs FreeType), DPI handling, shadows.
