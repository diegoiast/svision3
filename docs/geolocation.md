# Geolocation — provider architecture

Status: **design only, no code written yet**. This document exists to settle the
shape of the abstraction before anyone implements it. It deliberately does not
decide how bytes get fetched over the network (see "Explicitly left unknown"
below) — that's a separate design pass once this shape is agreed on.

## Goal

Give app developers a way to ask "where is the user, roughly?" that:

- Uses the OS-native location broker when one exists (Windows/macOS/Linux each
  have one, with different capability/precision/permission models).
- Falls back to something that still works when the native broker is
  unavailable, denied, or absent (common on Linux — see below).
- Follows this toolkit's existing conventions instead of inventing new ones
  where an existing pattern already fits.

## Precedent: two existing patterns, and why neither fits alone

This toolkit already has two provider-injection patterns for a similar
problem (image loading). Both are documented here because the new
`LocationProvider` design deliberately combines them — it isn't a copy of
either one.

### Pattern A — `ImageLoaderInterface` / `SVGLoaderInterface` (no override)

`include/toolkit/image.hpp`. Each `PlatformApplication` subclass lazily
constructs its own default and hands it out via a virtual getter:

```cpp
// include/toolkit/platform.hpp
virtual std::shared_ptr<ImageLoaderInterface> get_image_loader() = 0;
```

`Win32PlatformApplication` returns a `Win32ImageLoader`; X11/Wayland/macOS
return a `StbImageLoader`. **There is no app-developer override** — whichever
the platform constructs is what you get.

### Pattern B — `IconProvider` (developer-injected, no native default)

`include/toolkit/application.hpp`:

```cpp
void set_icon_provider(std::unique_ptr<IconProvider> provider);
IconProvider *icon_provider() const;
bool use_xdg_icons(); // convenience: auto-detect + install if found, else no-op
```

`Application` starts with a `DummyIconProvider` (always returns nothing,
never crashes). There's no OS-native default at all here — XDG icon themes
aren't an OS API, they're a desktop-environment convention, so the app
developer opts in explicitly (`use_xdg_icons()` for auto-detection, or
`set_icon_provider(...)` for an explicit one, as `demo1.cpp` does with
`XdgImageLoader`).

### Why `LocationProvider` needs both

Unlike icon lookup, geolocation genuinely has an OS-native default worth
using automatically (Pattern A) — but unlike image loading, the app developer
also has a real reason to override it (Pattern B): they may want to force the
IP-based fallback for testing, skip the native permission prompt entirely, or
run on a Linux box with no GeoClue installed. So this design borrows the
getter from Pattern A *and* the override/default-safety from Pattern B.

## Why this isn't "per-platform, like `Painter`"

`Painter` (`CairoPainter` vs `GLPainter`) is a same-platform, user-selectable
*rendering strategy* — both implementations are compiled and usable on the
same OS, chosen via `SVISION_PAINT`. Location providers split along a
different axis:

- **Native providers are genuinely one-per-OS**, like `PlatformWindow`
  (X11 vs Wayland vs Win32 vs macOS) — compiled conditionally, not a user
  choice.
- **IP-based providers are not platform-specific at all.** An HTTP call to a
  geolocation API, or a lookup against a local MMDB file, involves zero
  platform APIs — the same C++ class works unchanged on all three OSes. There
  is no "Win32IpLocationProvider."

So the implementation list is **N native (one per OS) + M cross-platform**,
not N×M.

## The interface

```cpp
// include/toolkit/geolocation.hpp

namespace toolkit {

struct GeoPosition {
    double latitude;
    double longitude;
    std::optional<double> accuracy_meters; // radius, if the source can estimate it

    // Place name fields. IP-based providers (live API and offline MMDB alike)
    // fill these in for free -- it's literally what a city-level IP database
    // returns alongside lat/lon. Native GPS-style providers (WinRT/CoreLocation)
    // do NOT: a raw GPS fix has no place name attached, and turning one into a
    // city requires a separate reverse-geocoding call (CLGeocoder on macOS,
    // MapLocationFinder on Windows, nothing standard on Linux) that is out of
    // scope for this design. So these are optional and provider-dependent --
    // callers must not assume they're populated.
    std::optional<std::string> city;
    std::optional<std::string> region; // state/province/subdivision
    std::optional<std::string> country;
};

enum class LocationError {
    PermissionDenied,  // user/OS declined
    Unavailable,       // no backend present (e.g. GeoClue not installed)
    Timeout,
    NetworkError,      // for network-backed providers
    Unknown,
};

using LocationResult = std::expected<GeoPosition, LocationError>;

class LocationProvider {
  public:
    virtual ~LocationProvider() = default;

    // Human-readable name for logging/diagnostics ("WinRT Geolocator",
    // "GeoClue2", "IP API (geojs.io)", "Offline MMDB") — surfaced so an app
    // can show/log which source actually answered.
    virtual std::string_view name() const = 0;

    // Always async: every real backend is either permission-gated, network-
    // bound, or both. One-shot only for v1 — no continuous tracking/watch API
    // yet (mirrors FileDialog's one-shot-per-call shape; can grow a
    // watch_location() later if a real use case shows up).
    virtual void request_location(std::function<void(LocationResult)> callback) = 0;
};

} // namespace toolkit
```

`std::expected` mirrors the error-carrying return already implied by
`LocationError`; if the project's baseline C++ standard/compiler support
makes `std::expected` inconvenient, `std::optional<GeoPosition>` plus a
separate error out-param is an acceptable fallback — not a blocking design
decision either way.

## Platform-native default: mirrors Pattern A

```cpp
// include/toolkit/platform.hpp — added alongside get_image_loader()
virtual std::shared_ptr<LocationProvider> get_location_provider() = 0;
```

Each `PlatformApplication` subclass lazily constructs and caches its own:

| Platform | Native backend | Notes |
|---|---|---|
| Windows | `WinRTLocationProvider` | Wraps `Windows::Devices::Geolocation::Geolocator` |
| macOS | `CoreLocationProvider` | Wraps `CLLocationManager`, needs `NSLocationWhenInUseUsageDescription` in the app's Info.plist |
| Linux | `GeoClueLocationProvider` | Talks to `org.freedesktop.portal.Location` over D-Bus, reusing the raw-`libdbus` pattern already vendored in `src/nfd/nfd_portal.cpp` |

If a platform has no native backend available at runtime (most notably:
Linux without GeoClue/xdg-desktop-portal installed), `get_location_provider()`
returns `nullptr` — same "safe absence" convention `DummyIconProvider` uses,
just expressed as a null pointer here instead of a no-op object, since the
caller needs to distinguish "no native provider" (to decide whether to fall
back) from "provider exists but denied/failed" (a `LocationResult` carrying
`LocationError`).

**This is not a hypothetical edge case — verified directly on the Debian/KDE
machine this design was written on.** `xdg-desktop-portal`,
`xdg-desktop-portal-gtk`, and `xdg-desktop-portal-kde` are all installed, but
`geoclue-2.0` is not, and nothing installed depends on it — Debian's KDE task
metapackage doesn't pull it in the way GNOME's does. So a working portal
daemon is not proof a working `Location` backend exists underneath it;
`get_location_provider()`'s "is this actually usable" check needs to be a
real probe, not just "did I find the D-Bus name." GeoClue2 itself, per its own
package `Recommends`, is a broker over `avahi-daemon` (network hints),
`iio-sensor-proxy` (hardware GPS/sensors), `modemmanager` (cellular GPS), and
`wpasupplicant` (WiFi-based positioning) plus an internal IP fallback — actual
accuracy varies by which of those a given machine happens to have.

### Real-world precedent: what Firefox and Qt actually do here

Worth knowing before implementing `GeoClueLocationProvider`, because both
data points argue for decisions already made in this design:

- **Qt** (`QtPositioning`, since Qt 5.12) implements Linux support as a named
  plugin, `geoclue2`, and has **no fallback of its own** — GeoClue2 or
  nothing. Qt's own documentation and forum threads warn that
  `QGeoPositionInfoSource::createDefaultSource()` "may not work as expected
  because geoclue2 might not be the default plugin," and recommend
  explicitly enumerating `availableSources()` and requesting `"geoclue2"` by
  name instead of trusting automatic selection. This directly validates
  `setup_native_provider()` being an explicit opt-in call rather than
  something `Application` does automatically on construction.
- **Firefox** relied on its own network-based positioning (Mozilla Location
  Service — sending WiFi/cell scan data to Mozilla's servers) as its
  Linux-independent fallback for years, only adding GeoClue2 as the preferred
  Linux default in Firefox 102 (2022). But **Mozilla Location Service stopped
  serving public/third-party requests on 2024-03-27** — it now returns HTTP
  403 to anyone outside Mozilla's own products. So Firefox's historical
  fallback is now dead weight for everyone else, and on Linux it's
  effectively down to GeoClue2 with nothing behind it either.

Neither of the two most mature reference implementations currently has a
working answer for "GeoClue2 isn't installed" on Linux. This toolkit's
`FallbackLocationProvider` chain (native → `IpApiLocationProvider` →
`OfflineMmdbLocationProvider`) is a genuine improvement on that point, not
redundant engineering — it's worth treating as a first-class part of this
design rather than an afterthought bolted onto the Linux native provider.

## Developer-facing API on `Application`: mirrors Pattern B

```cpp
// include/toolkit/application.hpp — added alongside set_icon_provider()

// Convenience: ask the current platform for its native provider and install
// it as the active one. Mirrors use_xdg_icons()'s shape exactly — returns
// false and leaves the active provider untouched if the platform has none.
bool setup_native_provider();

// Explicit override — works with ANY LocationProvider, native or not. This is
// how an app installs IpApiLocationProvider, OfflineMmdbLocationProvider, a
// custom fallback-chain composite, or a test double.
void set_provider(std::unique_ptr<LocationProvider> provider);
LocationProvider *provider() const;
```

Default before either is called: a `NullLocationProvider` that always
resolves with `LocationError::Unavailable` — same safety net
`DummyIconProvider` provides, so calling code never has to null-check.

## Cross-platform providers (compiled everywhere, injected via `set_provider`)

These implement the same `LocationProvider` interface but contain **no
platform-specific code at all** — they're ordinary portable C++, available on
every OS build, per the "why this isn't per-platform" section above.

- `IpApiLocationProvider` — calls a live IP-geolocation HTTP API, parses
  JSON, returns a `GeoPosition` (with `city`/`region`/`country` filled in —
  both candidate APIs return them alongside lat/lon):
  - Primary: `https://get.geojs.io/v1/ip/geo.json` (own IP) or
    `https://get.geojs.io/v1/ip/geo/{ip}.json` (specific IP) — no API key, no
    stated rate limit.
  - Secondary/fallback: `https://ipwho.is/` (own IP) or `https://ipwho.is/{ip}`
    — no API key, 1,000 requests/day.
- `OfflineMmdbLocationProvider` — resolves the machine's own public IP, then
  looks it up against a local MMDB file with no per-request network call. See
  sketch below.

Neither is wired up as anyone's default; an app developer opts in with
`set_provider(std::make_unique<IpApiLocationProvider>(...))`, or composes a
fallback chain itself (see next section).

### URLs this design assumes (subject to the third parties' own ToS/uptime)

| Purpose | URL | Auth |
|---|---|---|
| Get own public IP (plain text response) | `https://api.ipify.org` or `https://icanhazip.com` | None |
| Live geolocation (primary) | `https://get.geojs.io/v1/ip/geo.json` | None |
| Live geolocation (fallback) | `https://ipwho.is/` | None |
| Offline DB download | `https://db-ip.com/db/download/ip-to-city-lite` | None |
| Offline DB download (CDN mirror) | `https://cdn.jsdelivr.net/npm/dbip-city-lite/dbip-city-lite.mmdb.gz` | None |

None of these require a signup/API key, which is exactly why they were the
recommendation earlier (see the IP-lookup and offline-DB discussions this
document distills) — a toolkit consumed by many downstream developers can't
bake in a personal account credential the way MaxMind's own GeoLite2
download would require.

### `OfflineMmdbLocationProvider` — minor sketch

Deliberately incomplete: the two network-fetch calls (`fetch_own_ip()`,
`refresh_database_if_stale()`) are declared but not implemented here — that's
the exact boundary "Explicitly left unknown" below is about. Everything else
— the MMDB query, staleness policy, and result shape — is concrete enough to
review now.

```cpp
class OfflineMmdbLocationProvider : public LocationProvider {
  public:
    explicit OfflineMmdbLocationProvider(std::filesystem::path mmdb_path,
                                          std::chrono::days max_age = std::chrono::days{30});

    std::string_view name() const override { return "Offline MMDB"; }

    void request_location(std::function<void(LocationResult)> callback) override {
        refresh_database_if_stale(); // no-op if already fresh; see below
        if (!db_loaded_) {
            callback(std::unexpected(LocationError::Unavailable));
            return;
        }
        fetch_own_ip([this, callback](std::optional<std::string> ip) {
            if (!ip) {
                callback(std::unexpected(LocationError::NetworkError));
                return;
            }
            callback(lookup(*ip)); // pure local MMDB_lookup_string() call, no network
        });
    }

  private:
    // TODO (separate design pass): actually perform the HTTP GET against
    // api.ipify.org / icanhazip.com and hand back the response body.
    void fetch_own_ip(std::function<void(std::optional<std::string>)> callback);

    // TODO (separate design pass): if mmdb_path_ is missing or older than
    // max_age_, download from the DB-IP URL above (to a temp file, then
    // atomic rename over mmdb_path_) before continuing. Must not block
    // request_location() forever if offline -- fall through to "use
    // whatever's on disk, however stale" rather than fail outright, per the
    // staleness discussion in the offline-DB write-up this doc distills.
    void refresh_database_if_stale();

    LocationResult lookup(std::string const &ip) const; // MMDB_lookup_string() + field extraction

    std::filesystem::path mmdb_path_;
    std::chrono::days max_age_;
    bool db_loaded_ = false;
    // MMDB_s handle (libmaxminddb) would live here once that dependency is added.
};
```

## Fallback chain: a new pattern, not borrowed from existing code

Neither `Painter` nor the image-loader patterns implement "try A, if it
fails/denies/times out, fall through to B" — both existing patterns commit to
exactly one implementation up front. A fallback chain is new for this
toolkit. Proposed shape: a `LocationProvider` implementation that itself
holds an ordered list of other providers and tries each in turn:

```cpp
class FallbackLocationProvider : public LocationProvider {
  public:
    explicit FallbackLocationProvider(std::vector<std::unique_ptr<LocationProvider>> chain);
    std::string_view name() const override; // name of whichever provider last answered
    void request_location(std::function<void(LocationResult)> callback) override;
    // Internally: try chain[0]; on any LocationError, try chain[1]; etc.
    // Succeeds as soon as one provider returns a GeoPosition, or exhausts
    // the chain and reports the last error.
};
```

A typical app would build: `{ setup_native_provider() result (if any),
IpApiLocationProvider, OfflineMmdbLocationProvider }` and wrap it in
`FallbackLocationProvider`, then `set_provider()` that composite as the
active one. This keeps `Application`'s override mechanism simple (still just
one `LocationProvider*`) while letting the chain policy live in its own
class, not baked into `Application`.

## Developer-facing façade: owned accessor, not a fresh-constructed builder

Unlike `FileDialog` (which has real per-call configuration — title, filters,
start path — so constructing a fresh one each call makes sense), a
geolocation request has no such config. So instead of mirroring
`FileDialog`'s "construct fresh, chain setters, call `.open()`" shape, this
mirrors `Application::icon_provider()` instead: a persistent object owned by
`Application`, handed out via a raw-pointer accessor, called through with
`->`:

```cpp
// include/toolkit/application.hpp
Geolocator *geolocator(); // lazily constructed, always non-null, owned by Application
```

```cpp
// include/toolkit/geolocation.hpp

class Geolocator {
  public:
    using Callback = std::function<void(LocationResult)>;

    class Future {
      public:
        explicit Future(std::shared_ptr<Callback> cb) : callback_(std::move(cb)) {}
        Future &then(Callback cb) {
            *callback_ = std::move(cb);
            return *this;
        }
      private:
        std::shared_ptr<Callback> callback_;
    };

    Future request(); // internally: app_.provider()->request_location(...)

  private:
    friend class Application; // only Application constructs one
    explicit Geolocator(Application &app);
    Application &app_;
};
```

Usage:

```cpp
app.geolocator()->request().then([](toolkit::LocationResult result) {
    if (result) {
        spdlog::info("lat={} lon={}", result->latitude, result->longitude);
    } else {
        spdlog::warn("location unavailable: {}", (int)result.error());
    }
});
```

`Geolocator` itself never needs swapping out — it's just the stable entry
point. What's swappable is the `LocationProvider` underneath it, via
`set_provider()`/`setup_native_provider()` on `Application` as already
described above.

## Full usage example

Ties together `setup_native_provider()`, `set_provider()`,
`FallbackLocationProvider`, and the optional `city`/`region`/`country`
fields from a single app's startup code:

```cpp
#include "toolkit/application.hpp"
#include "toolkit/geolocation.hpp"

int main() {
    toolkit::Application app;

    // Try the OS-native broker first (WinRT / CoreLocation / GeoClue-via-portal).
    // Returns false if this platform has none installed/available -- not an
    // error, just "nothing to install."
    bool has_native = app.setup_native_provider();
    spdlog::info("native location provider available: {}", has_native);

    // Whether or not a native provider is active, install a fallback chain so
    // permission-denied / unavailable / timeout still resolves to *something*:
    std::vector<std::unique_ptr<toolkit::LocationProvider>> chain;
    if (has_native) {
        // app.provider() currently holds the native one from setup_native_provider();
        // move it in as the first link in the chain.
        chain.push_back(app.take_provider());
    }
    chain.push_back(std::make_unique<toolkit::IpApiLocationProvider>());
    chain.push_back(std::make_unique<toolkit::OfflineMmdbLocationProvider>("/path/to/dbip-city-lite.mmdb"));
    app.set_provider(std::make_unique<toolkit::FallbackLocationProvider>(std::move(chain)));

    auto *window = app.create_window("Weather", {400, 300});

    app.geolocator()->request().then([window](toolkit::LocationResult result) {
        if (!result) {
            spdlog::warn("location unavailable: {}", (int)result.error());
            return;
        }
        // city/region/country are only set if the answering provider supplies
        // them (any IP-based one does; a native GPS fix alone does not).
        if (result->city) {
            spdlog::info("near {}, {} ({:.4f}, {:.4f})", *result->city,
                         result->country.value_or("?"), result->latitude, result->longitude);
        } else {
            spdlog::info("at ({:.4f}, {:.4f}), no place name available",
                         result->latitude, result->longitude);
        }
    });

    window->show();
    return app.run();
}
```

Note: `app.take_provider()` (move the currently-installed provider out,
leaving `NullLocationProvider` in its place) isn't listed in the API sketch
above — it's a small addition needed so the native provider can be
relocated into a `FallbackLocationProvider` chain instead of just discarded;
add it alongside `set_provider()`/`provider()` when this is implemented.

`Geolocation::request()` internally just calls
`app_.provider()->request_location(...)` — the façade adds nothing but the
fluent/`.then()` shape consistency; all the actual provider-selection logic
lives in `Application`/`LocationProvider`, not here.

## Explicitly left unknown (separate design pass)

Per explicit instruction: **how bytes get fetched over the network is not
decided by this document.** Both `IpApiLocationProvider` (needs one HTTP GET
for the geolocation API response) and `OfflineMmdbLocationProvider` (needs
one HTTP GET for its own public IP, plus a periodic HTTP GET to refresh the
`.mmdb` file) have a "fetch this URL, get bytes back" dependency that is
deliberately left as a gap here — no HTTP client library has been chosen, no
decision has been made about whether fetching is synchronous-on-a-thread,
callback-based, or something else, and no decision has been made about where
the downloaded `.mmdb` file lives on disk or how its refresh is scheduled.

The `LocationProvider` interface above is designed so that gap doesn't leak
upward: both network-backed providers do their own fetching internally and
only ever hand the `Application`/`Geolocation` layer a finished
`LocationResult`. Whatever the network layer ends up looking like, it should
not require changes to `LocationProvider`, `Application`, or `Geolocation`
as designed here — only to the internals of the two concrete providers.

## Open questions for whoever picks this up next

1. Confirm `std::expected` availability, or fall back to
   `std::optional<GeoPosition>` + separate error accessor.
2. Decide the network-fetch abstraction (separate design doc) — likely
   candidates: a minimal libcurl wrapper, or reusing whatever the toolkit
   ends up needing for other network features, if any exist by then.
3. Decide the MMDB reader dependency (`libmaxminddb` vs. a minimal in-house
   reader) — leaning `libmaxminddb` per the earlier discussion (BSD-2-Clause,
   small, works with DB-IP's MMDB files despite the MaxMind-branded name).
4. Decide where a downloaded `.mmdb` file is cached on disk per-platform
   (likely alongside whatever convention the toolkit already uses for
   per-app cache/config directories, if one exists — not checked as part of
   this document).
5. macOS `Info.plist` usage-description string: does this toolkit have an
   existing mechanism for apps to supply Info.plist entries, or does this
   need one added as a prerequisite for `CoreLocationProvider` to work at
   all?
6. Linux: decide whether `GeoClueLocationProvider` talks to the portal
   (`org.freedesktop.portal.Location`, sandboxed-app-friendly, permission
   dialog "for free") or GeoClue2 directly (works without
   `xdg-desktop-portal` installed, no portal permission UI — but note neither
   path helps if `geoclue-2.0` itself isn't installed, per the verified
   finding above). Current lean is the portal, for consistency with
   `nfd_portal.cpp`'s existing pattern — not finalized here.
7. Reverse geocoding for native providers is explicitly out of scope for this
   design — `WinRTLocationProvider`/`CoreLocationProvider` will leave
   `city`/`region`/`country` unset. If an app needs a place name from a GPS
   fix specifically (not just from the IP-based providers, which already
   supply one), that's a separate future capability
   (`ReverseGeocodingProvider`?), not something to bolt onto
   `LocationProvider` itself.
