# Xenon IE

Internet Explorer for Xbox 360's UI rebuilt as a homebrew title on
[xenosworks/xsdk](https://github.com/xenosworks/xsdk): dark chrome, the Web Hub
(favorites / recent / featured tiles), address-bar edit with suggestions and the
full on-screen keyboard, hint bar and controller navigation — matched to the
reference screenshots. The page view is a real but deliberately small web engine:
HTTP/1.1 over BSD sockets with redirects and chunked bodies, plus an HTML subset
renderer (title, h1-h3, p, li, br, a, img alt). No external rendering engine.

## Layout

- `shaders/` — the one quad shader pair used for every filled rectangle.
- `src/theme.h` — palette + 720p geometry (tweak to retune the look).
- `src/gfx.*` — frame begin/end, `fill_rect`, text measure/wrap with a
  per-glyph width cache built on `xsdk_text_measure`.
- `src/input.*` — gamepad wrapper: edge detection, dpad/stick nav auto-repeat.
- `src/store.*` — favorites/history on `game:/ie_fav.txt`, `game:/ie_hist.txt`.
- `src/keyboard.*` — the on-screen keyboard (lower/upper/symbols/accents).
- `src/addressbar.*` — top chrome, edit field with selection + caret, suggestions.
- `src/hub.*` — Web Hub tabs and tile strip.
- `src/browser.*` — HTTP fetch + HTML subset renderer + page view.
- `src/main.cpp` — app state machine (Hub / Page / Edit), hints, about box.

## Controls

- Hub: dpad up/down tabs, left/right tiles, A open, X edit favorites
  (A removes), Y address field, B hide hub (with a page open).
- Page: right stick scrolls, dpad up/down walks links, A opens link,
  X favorite/unfavorite, Y address field, B back / hub.
- Edit: keyboard (dpad + A, hold A to repeat), up from top row reaches the
  suggestion tiles, A on a tile or ENTER navigates, B cancels.
- Back: exit the title.

## Build (local, Debian/Ubuntu or WSL)

```sh
git clone https://github.com/xenosworks/xsdk.git xsdk
bash xsdk/tools/toolchain/build.sh # GCC cross toolchain to ~/xsdk-gcc (first time only)
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$PWD/xsdk/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Produces `build/xenon_ie.xex`. Copy it to the console (RGH/JTAG or Xenia Canary
for a smoke test) and run; `xsdk_log_write` output and the `game:/` store files
land next to the title.

## Build (CI)

`.github/workflows/xex.yml` builds on `ubuntu-latest`: clones xsdk itself,
restores a cached `~/xsdk-gcc` toolchain (built by `xsdk/tools/toolchain/build.sh`
on cache miss), configures with xsdk's CMake toolchain file, builds, and uploads
the `.xex` as an artifact. No git submodule needed — just push the project files.

## Scope and notes

- **HTTP only.** HTTPS would need mbedtls-xenon; https:// URLs show a clear
  error page instead of a silent failure.
- Opaque rectangle fills only (no blend state), matching the screenshots' flat
  panels; the font glyphs used (arrows ← → ↻, geometric ● ◆ ▲, Latin-1 accents)
  are all in xsdk's 10x20 font coverage.
- Written against xsdk's documented API (`xsdk_text_*`, `xsdk_present_*`,
  `xsdk_gpu_*`, `XamInputGetState`, BSD sockets). If a pinned xsdk commit
  renamed something, the fix is a one-liner at the call site.
- `xsdk_gpu_set_vertex_constants(first, floats, count)` is called with
  `count` in float constants (vec4s): the quad's push block is exactly
  `vec4 rect + vec4 inv_view` = 2 constants. If your xsdk revision takes a float
  count instead, pass 8 in `gfx.cpp::fill_rect`.
