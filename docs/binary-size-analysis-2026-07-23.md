# Why is demo still ~4.8 MB? A link-time size breakdown

Date: 2026-07-23
Binary analyzed: `build/Release/demo` (Release, after moving `harfbuzz`/`freetype` to
dynamic system libraries per `conanfile.py`)

## TL;DR

| | Size |
|---|---|
| `demo`, unstripped | 5.75 MB |
| `demo`, stripped | **4.78 MB** |
| `.text` (code) | 4.63 MB |

Of the ~3.95 MB of `.text` we could attribute to a specific symbol (~85% of
`.text`; the rest is alignment padding, PLT stubs, and unnamed local data),
the single biggest contributor is **our own toolkit code (41%)** — not a
third-party dependency. The rest is a full HTML/CSS engine, an SVG renderer,
and a logging library, all still statically linked. Nothing here points to a
misconfigured build; it's the honest cost of what's linked in.

## Methodology

Archive sizes on disk (`ls -la libfoo.a`) overstate what actually ends up in
the binary, because static linking only pulls in the `.o` members a program
actually references — an archive with 50 object files might only contribute
5 of them. To get real numbers instead of a theoretical upper bound:

1. Re-ran the exact link command CMake used for `demo` (captured from
   `CMakeFiles/demo.dir/link.txt`), without stripping, to get an unstripped
   binary with a full symbol table.
2. Built a symbol → source-library map by running `nm -C` over each static
   archive (`libtoolkit.a`, `liblitehtml.a`, `libgumbo.a`, `liblunasvg.a`,
   `libplutovg.a`, `libspdlog.a`, `libfmt.a`, `libmd4c*.a`) individually.
3. Ran `nm --print-size --size-sort -C` on the final linked binary and
   looked up every defined symbol against that map, summing sizes per
   library. This reflects reality: e.g. it correctly shows the bundled
   sqlite3 module contributes **0 bytes** to `demo`, even though
   `sqlite3.c.o` is the single largest object file inside `libtoolkit.a`
   (1.78 MB) — nothing in `demo1.cpp` references it, so the linker never
   pulls that member in.

## Breakdown (by real, link-time contribution)

| Library | Size | % of attributed `.text` | What it is |
|---|---:|---:|---|
| **toolkit** (our widgets/platform/theme code) | 1663.7 KB | 41.1% | Everything under `src/toolkit/` |
| litehtml | 581.6 KB | 14.4% | HTML/CSS layout+render engine (`HtmlView`, Markdown preview) |
| app / libc / libstdc++ / other | 387.0 KB | 9.6% | `main()`, exception tables, **`std::regex`** (see below) |
| toolkit (vendored **stb_image**) | 307.1 KB | 7.6% | Bundled `stb_image.h`/`stb_image_write.h` (PNG/JPEG/BMP codecs) |
| gumbo | 297.0 KB | 7.3% | HTML5 tokenizer litehtml depends on |
| lunasvg | 256.2 KB | 6.3% | SVG rendering (icon loading) |
| spdlog | 181.2 KB | 4.5% | Logging |
| plutovg | 168.4 KB | 4.2% | 2D rasterizer lunasvg renders through |
| fmt | 107.4 KB | 2.7% | Formatting library spdlog uses |
| md4c (+ md4c-html) | 96.9 KB | 2.4% | Markdown parser |

Grouped by feature:

- **HTML/CSS engine** (litehtml + gumbo): 878.6 KB (21.7%)
- **SVG stack** (lunasvg + plutovg): 424.6 KB (10.5%)
- **Logging** (spdlog + fmt): 288.6 KB (7.2%)
- **Toolkit + vendored stb_image**: 1970.8 KB (48.7%)
- **Markdown**: 96.9 KB (2.4%)

## What's actually inside "toolkit" (1.66 MB)

This isn't one bloated file — it's the natural sum of a fairly complete
widget toolkit. Top real contributors in the final binary include:

- `XdgImageLoader::parse_index_theme` (~15 KB) — icon theme index parsing
- File browser sorting templates (`std::__introsort_loop` instantiations for
  `DirEntry`/`DirItem`) — several KB each, one instantiation per comparator
  lambda in `FileBrowserWidget`/`DirectoryDialog`
- `Window::draw_widget_inspector` (~9.4 KB)
- `LineInput::LineInput` (~8.8 KB)
- Widget classes across `TableView`, `TreeView`, `ListView`, `IconGrid`,
  `TextEdit`, `Splitter`, `TabWidget`, `MenuBar`, five theme implementations
  (win95/win11/macos/plasma/base), and — notably — **both** platform
  backends compiled in simultaneously:
  - X11 backend: ~36.5 KB
  - Wayland backend: ~40.2 KB

  `platform_factory.cpp` picks one at runtime based on environment, but
  since both are referenced from the same factory function, the linker has
  no way to know only one will ever run — both ship in every binary.

## The `std::regex` surprise

`demo1.cpp` uses `std::regex_match` for two input validators (numbers-only,
email format). Naively this looks tiny — but libstdc++'s `<regex>` header is
notorious for pulling in a full NFA-based regex compiler plus Unicode
grapheme-boundary tables regardless of how simple the pattern is. In this
binary that shows up as several KB of `std::__unicode::__v16_0_0::*` tables
and `std::__detail::_Compiler<...>::_M_insert_*` template instantiations —
a meaningful chunk of the 387 KB "app/other" bucket for two fairly simple
patterns. If binary size mattered more than convenience here, hand-rolled
validation or a lighter regex engine would remove this entirely.

## Confirmed: harfbuzz/freetype are dynamic now

Neither appears anywhere in the symbol map built from the static archives
that got linked — `ldd demo` shows `libharfbuzz.so.0` and `libfreetype.so.6`
resolved against system libraries, exactly as intended from the
`conanfile.py` change.

## If you want it smaller

In order of expected impact:

1. **`-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`** (or LTO)
   — the current build has no fine-grained dead-code elimination *within*
   an object file, only at the whole-`.o`-member level. Enabling this could
   shave real weight off litehtml/gumbo/lunasvg, since large source files
   like `document.cpp.o` (221 KB in the litehtml archive) likely have
   functions that never end up called from this app's usage of `HtmlView`.
2. **Build only the platform backend actually needed** instead of compiling
   both X11 and Wayland into every binary (~77 KB combined, small in
   absolute terms but 100% avoidable per deployment target if you know the
   target windowing system ahead of time).
3. **Move `litehtml`/`lunasvg` to system dynamic libraries** the same way
   `harfbuzz`/`freetype` were, if your target distro packages them —
   would save ~1.0–1.3 MB the same way the harfbuzz/freetype change did,
   though these aren't as universally packaged as harfbuzz/freetype.
4. Drop `std::regex` in favor of hand-written validation for the two simple
   patterns in `demo1.cpp` — a small, low-effort win specific to this demo.

None of these are "the build is broken" fixes — they're the normal
trade-offs of static-linking a full HTML engine, SVG renderer, and logging
library into a GUI toolkit demo.
