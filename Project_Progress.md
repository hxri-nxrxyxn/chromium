# Project Progress — Distraction-Free Chromium Fork

## Architecture Overview

Two independent subsystems work together to create a distraction-free experience:

| Layer | Component | Purpose |
|-------|-----------|---------|
| 1 | `ShortsReelsBlockerThrottle` | Hard-block short-form video feed URLs before any network request |
| 1 | `ShortsReelsBlockerTabHelper` | Intercept same-document SPA navigations (pushState/replaceState) |
| 1 | `chrome://distraction-blocked` | WebUI block page shown to the user |
| 2 | `ContentInjectionManager` | Inject CSS/JS at DOMContentLoaded into the primary main frame |
| 1&2| `platform_rules/` & `platforms/` | Modular, per-platform rule files aggregated at compile time |

### Block Rules (Layer 1)

| Type | Examples |
|------|---------|
| Prefix | `/shorts`, `/reels`, `/videos/reels`, `/foryou` |
| Regex | Reddit share links like `r/*/s/*`, complex X.com paths |

### Injection Rules (Layer 2)

| Site | Effect |
|------|--------|
| instagram.com/ | Hide Stories tray (multiple fallback selectors) |
| instagram.com/explore | Hide algorithmic grid |
| facebook.com | Hide Reels and Watch shelves |
| youtube.com | Hide Shorts shelf on home and search pages |
| reddit.com/ | Hide subreddit recommendation widgets |
| linkedin.com/ and /feed | Hide LinkedIn feed content |
| x.com/home | MutationObserver removes "For You" tab |
| x.com/explore | Hide trending/algorithmic content |
| tumblr.com/dashboard/stuff_for_you | Remove "Stuff for You" panel |

### Android NTP Changes

- Feed surface (`FeedSurfaceCoordinator`) replaced with a no-op stub
- Sign-in promo removed
- Compose button removed
- "Stuff for You" section removed

---

## Code Quality Milestones

### Session 2026-05-29 — Full Code Quality Audit & Fixes

**15 issues identified, all resolved:**

#### Critical Bugs Fixed

| # | File | Issue | Fix |
|---|------|-------|-----|
| 1 | `shorts_reels_blocker.cc` | TOCTOU race: `fetch_add` + separate `load` gave wrong block count | Use return value of `fetch_add` directly |
| 2 | `NewTabPage.java` | `getScrollDelegate()` returned `null` → NPE in coordinator | Return no-op `FeedSurfaceScrollDelegate` stub |
| 3 | `NewTabPage.java` | `getUiConfig()` returned `null` → NPE on tablet (`addObserver`) | Construct real `UiConfig` from root view |
| 4 | `NewTabPage.java` | Test methods cast stub to `FeedSurfaceCoordinator` → `ClassCastException` | Override to throw `UnsupportedOperationException` |

#### Code Quality Improvements

| # | File | Change |
|---|------|--------|
| 5 | `shorts_reels_blocker.cc` | Extracted `CheckAndMaybeBlock()` helper; `WillStartRequest` and `WillRedirectRequest` now share one implementation |
| 6 | `shorts_reels_blocker.cc` | Fixed duplicate CSS property in `@media(max-width:420px)` block |
| 7 | `distraction_blocked_ui.cc` | Removed unused `url/gurl.h` include; fixed same duplicate CSS |
| 8 | `content_injection_manager.cc` | Replaced `JSONWriter`-encoded JS string with Base64 data-URI CSS injection: `base64::Encode(css)` → `<link rel=stylesheet href='data:text/css;base64,...'>` injected via isolated-world IIFE. Zero escaping risk, DevTools-visible, immune to page script interference. |
| 9 | `content_injection_rules.cc` | Added `aria-label="Stories"` + `:has(canvas)` fallback selectors for Instagram |
| 10 | `content_injection_rules.cc` | Normalized rule table column alignment |
| 11 | `GoogleServicesSettings.java` | Replaced always-false `shouldShowAllowSignIn()` branch with direct `setVisible(false)` + intent comment |
| 12 | `IdentityDiscController.java` | Added intent comment to `get()`; short-circuited `setProfile()` to skip wasteful `ProfileDataCache`, `IdentityManager`, and `SyncService` observer registration that can never affect the always-hidden button |
| 13 | `apply-patches.py` | Full rewrite: `argparse`, `--help`, `validate_args()`, `die()`, try/except on all file I/O |
| 14 | `INTEGRATION.md` | Rewrote as proper Markdown with code blocks (was shell-comment style) |
| 15 | `shorts_reels_blocker.h` | Added `[[nodiscard]]` and declaration for new `CheckAndMaybeBlock()` |
| 16 | Rule Aggregation | Split monolithic rule arrays (`shorts_reels_blocker.cc` and `content_injection_rules.cc`) into 14 distinct per-platform `.cc`/`.h` files across two `platforms/` directories, merged via `NoDestructor` aggregators. Engine remains unchanged. |

---

## Design Decisions

- **Data-driven rules** — No logic changes needed to add/remove blocked URLs or injected CSS. Edit arrays only.
- **CSS via Base64 data-URI** — CSS payload is `base64::Encode`d and injected as a `<link rel=stylesheet href='data:text/css;base64,...'>` element via an isolated-world IIFE at `DOMContentLoaded`. No escaping risk; DevTools-visible; immune to page script tampering.
- **JS via isolated world** — `ISOLATED_WORLD_ID_CONTENT_END` prevents the page from accessing or interfering with MutationObserver injections.
- **Two-layer blocking** — The throttle catches standard navigations; the TabHelper catches SPA same-document navigations.
- **IdentityDiscController** — `get()` always returns `canShow=false`; `setProfile()` skips all observer setup. Google sign-in is not supported in this fork.

---

## Next Steps

- [ ] Add unit tests for `ShortsReelsBlockerThrottle::CheckURL()` — the rule table is testable in isolation
- [ ] Investigate rebasing `NewTabPage.java` against the latest upstream to ensure stub compatibility
- [ ] Consider whether `kInstagramHomeCSS` `:has()` selector needs a `@supports` guard for older WebViews
- [ ] Evaluate adding a `chrome://distraction-free-settings` WebUI for rule management at runtime

---

## Execution

```sh
# Apply all patches to a Chromium checkout:
python3 apply-patches.py /path/to/chromium/src /path/to/this/repo

# Verify the script itself:
python3 -m py_compile apply-patches.py
python3 apply-patches.py --help
```
