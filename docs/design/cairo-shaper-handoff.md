# Cairo/Linux `TextShaper` — implementation handoff

Status: **written, never compiled, never run**. I (the agent that wrote this) have no
Linux machine, no Cairo/HarfBuzz/FreeType/fontconfig headers, and no way to invoke a
compiler against this code at all. Everything below is reasoned from documentation and
from this repo's existing patterns (`win32_shaper.cpp`, `cairo_painter.cpp`), not from a
working build. Treat this as a strong first draft that needs a real compile-and-run pass
on Linux before it's trustworthy, not as tested code.

This is the Linux/Cairo counterpart of `src/toolkit/painters/win32_shaper.cpp` (the
Windows/Uniscribe `TextShaper`), implementing requirement from
[`docs/design/rtl-line-input.md`](rtl-line-input.md) section 6 ("Cairo / Linux
(`cairo_shaper.cpp`)"). Read that doc first for the overall RTL/BiDi architecture
(`bidi::BidiLine`, `text::TextLayout`, the `TextShaper` interface contract) — this
document only covers the Cairo-specific backend.

---

## 🚦 Start here — you're a different agent, on a different (Linux) machine

If you're reading this, you're picking up where I left off, almost certainly in a fresh
session with no memory of writing the code below. I wrote `cairo_shaper.cpp`/`.hpp` and
wired it into the build entirely from documentation and pattern-matching against
`win32_shaper.cpp` — I had no way to compile or run any of it. Your job is to make it
*actually work*: build it, fix what's broken, verify it against the specific risks I
already flagged, and correct this document where I got something wrong. Don't take
anything below on faith just because it's written in a confident tone — confidence here
reflects how carefully I reasoned about an API, not whether the code runs.

### Read in this order before touching code

1. [`rtl-line-input.md`](rtl-line-input.md) — the whole feature's design. You need this to
   understand `bidi::BidiLine`, `text::TextLayout`, and the `TextShaper` interface
   contract that `CairoShaper` implements. Don't skip this even if it looks long; the
   Cairo-specific notes below assume you already know what a "run" is and why direction
   is forced rather than re-derived.
2. The rest of *this* document, start to finish — especially "Specific things that need
   real-device verification" below. That section is your actual punch list.
3. `src/toolkit/painters/win32_shaper.cpp` — the reference implementation. It implements
   the *exact same* `TextShaper` interface, is fully working and tested (on Windows), and
   every structural decision in `cairo_shaper.cpp` was deliberately made to mirror it.
   When something in the Cairo code is unclear or looks wrong, check what the Windows
   version does for the same situation before assuming the Cairo version is broken in a
   *new* way — it's more likely a translation mistake of an existing, correct pattern.

### What's done vs. what's yours

| Piece | Status |
|---|---|
| `bidi::BidiLine`, `text::TextLayout`, `TextShaper` interface (portable) | Done, tested, working. Don't change unless you find a bug that also affects Windows — if so, fix it for both platforms and say so clearly when you report back. |
| `LineInput` direction modes (Auto/LTR/RTL), keyboard toggle, context menu submenu, arrow-key logical movement | Done, tested, working (on Windows; the logic itself is platform-independent C++ in `line_input.cpp`/`bidi.cpp`, so it should just work once a shaper exists). Not what you're here for. |
| `win32_shaper.cpp` (Windows/Uniscribe backend) | Done, tested, working. Reference implementation — see above. |
| `cairo_shaper.cpp`/`.hpp` (Linux/Cairo backend) | **This is your job.** Written blind, never compiled. |
| `cocoa_shaper.mm` (macOS/CoreText backend) | **Not started.** Not in scope for you — don't let test failures or build issues on a Mac (if you somehow end up running there) distract you; this repo has no Cocoa shaper at all yet. |

### Your runbook

Work through these in order; each builds on the previous one passing.

1. **Get dependencies resolved.** `conan install . --build=missing` (or whatever the
   project's actual Conan invocation is — check for a `build_*.sh`/Makefile/CI config
   first). Then CMake-configure as usual. If `find_package(harfbuzz)` /
   `find_package(Freetype)` don't produce the `harfbuzz::harfbuzz` /
   `Freetype::Freetype` targets I assumed in `CMakeLists.txt`, inspect
   `build/generators/*.cmake` for the real exported target names and fix the
   `find_package`/pkg-config block accordingly (see risk item 4 below).
2. **Build `toolkit` alone first** (not `tests`/`demo` yet) — isolates `cairo_shaper.cpp`
   compile errors from everything else. Fix them one at a time. Common things likely to
   be wrong, roughly in order of how much I'd bet on each: a header name or include path
   (`<cairo-ft.h>`, `<hb-ft.h>`, `<freetype/freetype.h>` — these are my best guesses at
   the conventional un-prefixed include paths, not verified), an API parameter I
   misremembered, or a missing link library.
3. **Build and run `tests`.** Confirm the full suite still passes at whatever the
   pre-existing baseline is (ask the human running you, or check git history / CI for
   the last known-good count) — this backend should be purely additive, so anything
   newly broken is almost certainly a real regression from this change, not flakiness.
4. **Build and run `demo`.** Go to the "Inputs" tab (`demo/demo1.cpp`) — there are
   pre-existing RTL and mixed-RTL/LTR `LineInput` fields there used for manual testing on
   Windows. Type/edit text in them, check alignment and caret movement.
5. **Work through every item in "Specific things that need real-device verification"
   below, one at a time**, using the concrete test strings I suggested for each. Don't
   just eyeball "does Hebrew look roughly right" — the listed risks are specific and each
   has a specific way to confirm or refute it.
6. **Now that you can compile and run things, write `tests/test_cairo_shaper.cpp`**
   (mirroring `tests/test_win32_shaper.cpp`'s structure and the scenarios listed under
   "What's missing" below). I explicitly skipped this because shipping an unverified test
   file is worse than shipping none — you don't have that excuse anymore.
7. **Update this document.** Strike through or rewrite anything you confirmed was fine,
   fix anything that was actually wrong (with what you changed and why), and add any new
   findings. This file should keep being the accurate, current source of truth for this
   backend, not a frozen snapshot of my first guess.

### Definition of done

- `toolkit`, `tests`, and `demo` all build clean.
- `tests` passes at the established baseline (no new failures).
- In the demo: a pure-RTL field renders right-aligned with correctly shaped/joined glyphs;
  a mixed Hebrew-or-Arabic+Latin field reorders correctly at the visual level; typing,
  arrow-key movement, selection, and the context-menu direction toggle all behave the same
  as the working Windows build for equivalent text.
- Every item in the verification list below has been explicitly checked (not just
  "probably fine") and this document reflects the real outcome.
- `tests/test_cairo_shaper.cpp` exists and passes.

### When you're done (or stuck)

Summarize for the human what you fixed, what you verified, and what (if anything) is
still broken or deferred — same as you'd do for any other piece of work, but be explicit
about which of *my* claims in this document turned out to be wrong, since that's
information the human (and whoever reads this doc next) needs.

---

## Files touched

- **New**: `src/toolkit/painters/cairo_shaper.hpp` / `.cpp` — the `CairoShaper` class.
- **`conanfile.py`**: added an explicit `freetype/2.13.3` requirement on Linux (version
  unverified — see the comment at the requirement itself).
- **`CMakeLists.txt`**: added `harfbuzz`/`Freetype` `find_package`/pkg-config wiring next
  to the existing `cairo`/`fontconfig` block; added `cairo_shaper.cpp` to
  `TOOLKIT_SOURCES` under `TOOLKIT_WITH_CAIRO`; linked the two new targets next to
  `cairo::cairo`/`fontconfig::fontconfig`.
- **`src/toolkit/platform/x11/x11_platform.hpp`/`.cpp`**: added a `CairoShaper
  app_shaper_;` member to `X11PlatformApplication` and a `set_shaper(&app_shaper_);` call
  in its constructor, right next to the existing `set_rasterizer(&s_rasterizer);`.
- **`src/toolkit/platform/wayland/wl_platform.hpp`/`.cpp`**: same pattern for
  `WaylandPlatformApplication`.
- **Not touched**: macOS. The design doc's Cocoa backend (`cocoa_shaper.mm`, CoreText) is
  a separate, unrelated piece of work — out of scope here.
- **Not written**: `tests/test_cairo_shaper.cpp`. See "What's missing" below for why and
  what it should cover.

## Why this needed real FreeType + HarfBuzz glue (and why that's risky)

`CairoTextRasterizer` (the *existing* Cairo text path, `cairo_painter.cpp`) draws via
Cairo's "toy" font API (`cairo_select_font_face` + `cairo_show_text`). That API does no
shaping at all — one Unicode codepoint maps to one glyph via a plain cmap lookup, no
ligatures, no Arabic joining forms. Drawing each HarfBuzz-shaped cluster individually
through `cairo_show_text` would silently *drop* all the shaping HarfBuzz just computed
(each call only sees an isolated 1-2 character substring, breaking joining context) —
that would look identical to (or worse than) the current non-shaped fallback, defeating
the entire point of this feature. So `draw_run` has to bypass the toy API and drive Cairo
with HarfBuzz's own glyph IDs and positions via `cairo_show_glyphs` — which requires a
real `cairo_font_face_t` built from a real `FT_Face`, which requires resolving a family
name to an actual font file. None of that infrastructure existed in this codebase before
this change (confirmed via repo-wide grep: zero pre-existing `FT_New_Face`/`hb_shape`
calls), so it's all new, unexercised code.

## Architecture / key decisions

### Script segmentation: HarfBuzz's own Unicode data, not fribidi

The design doc says "fribidi to get per-run script". I could not find any fribidi API
that does script segmentation (fribidi is a bidi-*level* resolver, like our own
`bidi::BidiLine` — it doesn't itemize by script). I think this line in the design doc is
either imprecise, or was written before `bidi::BidiLine` existed as a from-scratch
in-repo implementation (the doc's own intro says "proposal, no code yet"). What's
actually needed is: a `bidi::BidiLine` run can still mix *scripts* at the same bidi level
(e.g. Hebrew directly followed by Arabic — both RTL, no level change, but two different
HarfBuzz shaping engines), and HarfBuzz needs an explicit, correct script per `hb_shape()`
call to apply script-specific features (Arabic joining is the big one).

Instead of fribidi, `cairo_shaper.cpp` calls `hb_unicode_script()` against HarfBuzz's own
default Unicode function table (`hb_unicode_funcs_get_default()`) — this gives the real
Unicode Script property per codepoint, already linked in via HarfBuzz, no extra
dependency. `segment_by_script()` (anonymous namespace, top of the file) walks the run
codepoint-by-codepoint and splits into maximal spans of one strong script, with
COMMON/INHERITED/UNKNOWN codepoints (digits, punctuation, whitespace, combining marks)
joining whichever span they sit in rather than starting a new one — same idea as how
bidi neutrals resolve to a surrounding strong direction.

**fribidi and freetype are still declared in `conanfile.py`** (fribidi explicitly,
freetype as harfbuzz's/cairo's transitive dependency) **but fribidi itself is unused** by
this implementation. If a real test run on Linux finds cases where `hb_unicode_script`
based segmentation produces visibly wrong joining/shaping, that's the place to revisit —
not a sign the approach is wrong in general, but check before reaching for fribidi as the
first fix.

### Per-span shaping + concatenation order (mirrors `win32_shaper.cpp`'s per-item logic)

For each script span (sliced from the run's *logical* UTF-8 in *logical* order):

1. Shape it with `hb_shape()`, with direction forced from the `rtl` parameter the caller
   already resolved via `bidi::BidiLine` (never re-derived here — same principle as
   `win32_shaper.cpp`'s "Uniscribe is only used for shaping here, never for re-deriving
   direction").
2. HarfBuzz's own contract guarantees the *output* glyph buffer for one `hb_shape()` call
   is already in final visual left-to-right order for whatever direction was set — no
   manual per-glyph reversal needed within one span (unlike Uniscribe, which needed
   `item_to_scalars`'s results manually reversed; HarfBuzz does this internally).
3. The spans themselves, however, were sliced from the run in *logical* order, and for an
   RTL run the last-typed span needs to render *leftmost* — so `shape_run`/`draw_run`
   concatenate/iterate the **span list** in reverse when `rtl` is true (same shape as
   `win32_shaper.cpp`'s `per_item.rbegin()` loop), while each span's own *internal* glyph
   order is left untouched.

This two-level structure (don't reverse within a span, do reverse across spans) is the
single most important thing to verify by eye on a real RTL+mixed-script string (e.g.
Hebrew followed directly by Arabic, no space, inside an English sentence) — it's exactly
the kind of off-by-one-level bug that's invisible in pure single-script text.

### Cluster grouping (one `ClusterAdvance` per HarfBuzz cluster)

`ClusterAdvance`'s documented v1 contract is "one cluster per Unicode scalar value (no
grapheme clustering/mark merging)". HarfBuzz's `hb_glyph_info_t.cluster` is the UTF-8 byte
offset (within the span passed to `hb_buffer_add_utf8`) that a glyph maps back to;
multiple glyphs can share one cluster (one base + combining marks → several glyphs, one
input position) or one glyph can represent multiple input codepoints (a ligature).
`advances_from_shape()` groups consecutive glyphs sharing a cluster value and sums their
`x_advance` into one `ClusterAdvance`, exactly mirroring how `win32_shaper.cpp`'s
`item_to_scalars()` groups multiple Uniscribe character positions sharing a byte offset.
Same documented approximation, same place it would show up (ligatures, combining marks)
if it's ever wrong.

### Font loading: fontconfig → FreeType, fresh per call, no cache

There was no existing FreeType handle anywhere in this codebase to reuse — confirmed via
grep. `CairoShaper::Impl` owns one persistent `FT_Library` (created once, like
`win32_shaper.cpp`'s persistent measurement-only `HDC`), but loads a *fresh* `FT_Face` per
`shape_run`/`draw_run` call via fontconfig (`FcNameParse` → `FcFontMatch` → `FC_FILE` +
`FC_INDEX` → `FT_New_Face`), same "simplicity over micro-optimization, v1" choice
`win32_shaper.cpp` makes with `CreateFontW`. **Caching `FT_Face`/`hb_font_t` by
`(family, pixel_size)` is the obvious follow-up perf win** once this is confirmed
correct — not done here to keep the first pass's surface area small.

Family name resolution mirrors `CairoTextRasterizer`'s `cairo_font_face()` *in spirit*
(`Theme::current().palette.fonts.system`/`.monospace`) but **not byte-for-byte**:
`CairoTextRasterizer`'s monospace path runs a runtime probe across a hardcoded candidate
list (`find_monospace_font()` in `cairo_painter.cpp`) measuring `'i'`/`'m'` advances,
because it only has Cairo's toy API to query with. `CairoShaper` instead hands
`palette.fonts.monospace` (e.g. the literal string `"monospace"`) straight to fontconfig,
which has its own generic-alias resolution for `"monospace"`/`"sans-serif"`/`"serif"` —
simpler and arguably more correct, but it means **in principle** `CairoShaper` could
resolve to a slightly different physical monospace font file than
`CairoTextRasterizer`'s probe picks, in environments with multiple monospace fonts
installed. Low-impact (font *metrics* would still be close), but worth a visual diff
check between a shaped and an unshaped monospace `LineInput` side by side.

**Assumed but not verified**: fontconfig is already initialized by the time any
`LineInput` paints (there's a pre-existing `FcFini()` call in `platform_factory.cpp` with
no corresponding explicit `FcInit()` visible in this codebase — fontconfig auto-inits
lazily on first real API use in most implementations, which is presumably why the
existing code gets away without an explicit init call, but this was not traced through to
confirm).

### Cairo's CTM already has the DPI scale — do not multiply by scale again

`GDIPainter::scale()` exists and `win32_shaper.cpp` multiplies `font_size` by it because
Win32's `GetHDC()` hands back a *raw* device HDC that doesn't carry the GDI+ `Graphics`
object's transform. **`CairoPainter` has no `scale()` method at all** — DPI scale is
baked into the live `cairo_t`'s current transformation matrix by the X11/Wayland backend
*before* the `CairoPainter` wrapping it is even constructed (confirmed: `cairo_scale(cr,
scale, scale)` in `X11CairoBackend.hpp`/the Wayland equivalent, ahead of every
`CairoPainter` construction). So `draw_run` intentionally does **not** multiply
`font_size` by anything — every Cairo draw call, including `cairo_show_glyphs`, is already
in logical/CSS pixels and Cairo's own CTM handles device-pixel mapping. If you see
shaped text rendering at the wrong physical size on a HiDPI Linux session, this
assumption is the first thing to re-check, not a missing multiply.

### Glyph index compatibility between HarfBuzz and Cairo

`cairo_glyph_t::index` must be a glyph ID in the *same* font's glyph table that HarfBuzz
shaped against. `draw_run` builds `cairo_font_face_t` via
`cairo_ft_font_face_create_for_ft_face()` from the *exact same* `FT_Face` that was handed
to `hb_ft_font_create_referenced()` earlier in the same call, so the index spaces are
guaranteed to match (glyph indices are a property of the font file's own glyph table,
shared by any library reading it, but using the identical `FT_Face` object removes any
doubt). This is the standard, well-documented HarfBuzz+Cairo integration pattern (the same
approach Pango uses internally) — I'm reasonably confident in the *shape* of this code,
less confident in typo-free correctness since nothing has compiled it.

## Specific things that need real-device verification (in rough priority order)

1. **Y-axis sign in `draw_run`** (`glyphs[i].y = y - positions[i].y_offset / 64.0`, and
   `y -= positions[i].y_advance / 64.0`). HarfBuzz's vertical axis is font/PostScript-style
   (up is positive); Cairo's is screen-style (down is positive) — the code negates both
   the per-glyph `y_offset` and the running `y_advance` to compensate. For purely
   horizontal Latin/Hebrew/Arabic text `y_advance` is 0 anyway so this mostly matters for
   `y_offset` on stacked diacritics. **Verify by rendering Hebrew text with niqqud or
   Arabic with full diacritics and checking the marks land above/below the base letter,
   not offset in the wrong direction.**
2. **The double-reversal structure** described above (don't reverse within a span, do
   reverse the span list when `rtl`). Verify with a string like `"Hello שלום עולם World"`
   (Hebrew embedded in English) and separately with Hebrew-directly-followed-by-Arabic
   with no separating space, to exercise both the run-level and script-span-level
   reversal independently.
3. **`cairo-ft.h` availability.** Assumed to ship as part of the main `cairo` package
   (true on essentially every Linux distro, since cairo is almost always built with
   FreeType support) — not separately probed in CMake. If the build fails on `#include
   <cairo-ft.h>` specifically, the fix is locating which package ships it on the target
   distro (it's normally the same `-dev`/`-devel` package as `cairo.h` itself).
4. **CMake target names.** `find_package(harfbuzz)` / `find_package(Freetype)` are
   guesses at what Conan's `CMakeDeps` generator exports (`harfbuzz::harfbuzz`,
   `Freetype::Freetype`) — **never confirmed against an actual `conan install` run**. If
   configure fails here, check `build/generators/*.cmake` for the real target names
   before reaching for the pkg-config fallback path (which itself assumes a
   `freetype2`/`harfbuzz` `.pc` file exists, also unverified). Also double check the
   `freetype/2.13.3` version pinned in `conanfile.py` doesn't conflict with whatever
   version harfbuzz pulls in transitively — drop the explicit pin if Conan complains.
5. **Script segmentation correctness on real mixed text.** The Hebrew+Arabic-no-space
   case above doubles as the test for this — confirm both scripts get correct joining
   *and* the segment boundary lands at the right byte offset (off-by-one here would
   misattribute one glyph's cluster to the wrong script's shaping engine).

## What's missing / explicitly out of scope

- **Bidi mirroring** (e.g. `(` / `)` / `[` / `]` flipping to their mirrored glyph form
  when they end up in an RTL run). Uniscribe does this automatically inside
  `ScriptShape` for `win32_shaper.cpp`; HarfBuzz does **not** auto-mirror — mirroring is
  the caller's responsibility in every HarfBuzz-based text stack I'm aware of (Pango does
  it explicitly before shaping). **Not implemented here.** Concretely: typing
  `"(hello)"` inside RTL context will currently render un-mirrored on Linux, while it's
  expected to mirror to `")hello("`-shaped parens on Windows via Uniscribe. This is a
  real, visible (but punctuation-only, limited-impact) gap relative to the Win32 backend.
  Two ways to close it: (a) add `fribidi_get_mirror_char()` (fribidi actually *is* the
  right tool for this specific narrow job, unlike script segmentation) and swap in the
  mirrored codepoint per-character before shaping when the run is RTL; (b) bundle a small
  lookup table from Unicode's `BidiMirroring.txt` (~500 entries) directly in this
  codebase and skip the fribidi dependency entirely. Either is a self-contained follow-up.
- **No font/glyph caching** (see above) — correctness-first v1, perf follow-up.
- **No `tests/test_cairo_shaper.cpp`.** `tests/test_win32_shaper.cpp` exists and tests
  `shape_run()`'s output directly (no live `Painter&`/window needed for that half of the
  interface). I did not write a Linux equivalent: with zero ability to compile *or run*
  it, a test file is more likely to ship its own undiscovered bugs than to catch real
  ones, and "looks plausible but never ran" tests are worse than no tests (false
  confidence). Recommended scenarios for whoever writes it, mirroring
  `test_win32_shaper.cpp`'s structure: pure-Latin run advances are sane and monotonic;
  pure-Hebrew run reverses correctly relative to logical order; a Hebrew-then-Arabic run
  (no space) produces two script spans at the correct byte boundary; `ClusterAdvance`
  byte offsets are valid `bidi::BidiLine::char_offsets()` entries for a representative
  mixed string (reusing the exact same reference strings `test_bidi.cpp` already uses,
  e.g. `"ABD אבג 123"`, would also cross-check the two layers agree).
- **macOS/Cocoa backend** — separate, untouched, not part of this change.

(See "Your runbook" near the top of this document for the concrete, ordered steps to
verify all of the above on a real machine.)
