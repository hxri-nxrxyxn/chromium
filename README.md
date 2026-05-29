# Distraction Blocker — Chromium for Android

A privacy-focused Chromium fork for Android that removes algorithmic feed distractions, blocks short-form video URLs, and strips unnecessary Google services from Settings and the New Tab Page.

---

## What's in here

| Area | What's changed |
|------|----------------|
| Navigation throttle | Blocks YouTube Shorts, Instagram/Facebook Reels, Reddit Reels, X Reels, LinkedIn Reels at the network level |
| Content injection | CSS/JS injected at DOMContentLoaded to suppress algorithmic feed UI on pages you're still allowed to visit |
| Custom error page | `chrome://distraction-blocked` — WebUI with block counter, renders instead of a dead tab |
| Settings | Stripped Google Services, Passwords, promo cards, sign-in elements — fixes NPE crash on open |
| New Tab Page | Logo + search bar only — no feed, tiles, composeplate, home modules, or sign-in promo |
| Notifications | Default-deny all notification permissions globally |
| Geist font | Set as system UI font |
| TikTok | Blocked entirely (all paths) |

---

## Layer 1 — Navigation Throttle

**Files:** `chrome/browser/navigation_policy/`

Hard-blocks short-form video feed URLs **before any network request is made**.
Also catches same-document SPA transitions (pushState/replaceState) the throttle never sees via `ShortsReelsBlockerTabHelper`.

**Per-platform rule files** — each platform has its own file:

| File | What it blocks |
|------|----------------|
| `platform_rules/all_block_rules.cc` | TikTok (all paths) |
| `platform_rules/youtube_block_rules.cc` | `/shorts/*` |
| `platform_rules/instagram_block_rules.cc` | `/reels/*` (allows `/<user>/reel/*`) |
| `platform_rules/facebook_block_rules.cc` | `/reels/*`, `/reel/*`, `/watch/*` |
| `platform_rules/reddit_block_rules.cc` | `/reels/*`, `/r/*/s/*` (regex) |
| `platform_rules/x_block_rules.cc` | `/i/reels/*` |
| `platform_rules/linkedin_block_rules.cc` | `/videos/reels/*` |

---

## Layer 2 — Content Injection

**Files:** `chrome/browser/content_injection/`

Injects CSS (preferred) or JS + MutationObserver (for heavy SPAs) at DOMContentLoaded to suppress algorithmic shelf/feed UI.

**Per-platform injection files:**

| File | Domain | What's removed |
|------|--------|----------------|
| `platforms/youtube_rules.cc` | youtube.com | Shorts shelf on home page |
| `platforms/instagram_rules.cc` | instagram.com | Stories tray (home), algorithmic grid + infinite scroll (explore) |
| `platforms/facebook_rules.cc` | facebook.com | Stories tray, Reels shelves in feed |
| `platforms/reddit_rules.cc` | reddit.com | Algorithmic home feed |
| `platforms/linkedin_rules.cc` | linkedin.com | Algorithmic feed stream (home, feed) |
| `platforms/x_rules.cc` | x.com | Timeline + nav bar (home), explore timeline + tab bar (explore) |
| `platforms/tumblr_rules.cc` | tumblr.com | Recommended posts (dashboard) |

### Design decisions

- **CSS over JS** wherever possible — no execution overhead, survives React re-renders, applies before first paint
- **JS + MutationObserver** only for X and Tumblr — they reconstruct the entire feed subtree on route changes
- **Isolated world** (`ISOLATED_WORLD_ID_CONTENT_END`): injected code cannot be tampered with by page JS
- **TabHelper at call site**, not inside throttle — independently testable classes
- **CheckURL reuse**: `TabHelper::MaybeBlockURL` calls `Throttle::CheckURL` directly — block logic lives in one place

---

## Custom Error Page

**Files:** `chrome/browser/ui/webui/distraction_blocked/`

When the throttle blocks a URL during navigation, it returns `ERR_BLOCKED_BY_CLIENT` with an inline error page. For SPA pushState navigations caught after commit, `NavigateToBlockPage()` redirects to `chrome://distraction-blocked`.

The WebUI is served inline (no GRIT) via `SetRequestFilter` and embeds a live block counter — `"blocked N times this session"`.

⚠️ **Registration required:** The WebUI must be registered in `chrome/browser/ui/webui/chrome_web_ui_configs.cc`:
```cpp
#include "chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h"
// Inside RegisterChromeWebUIConfigs():
map.AddWebUIConfig(std::make_unique<DistractionBlockedUIConfig>());
```

---

## Settings Modifications

The following were stripped from Settings to remove NPE crashes and unnecessary Google integration:

| What was removed | Why |
|------------------|-----|
| Google Services section | Crashed on open after account preference removal |
| Passwords section | NPE on `mMediator` in HistorySyncCoordinator |
| Promo card | NPE in promo card initialization |
| Sign-in preference | Empty account category after services removal |
| "Allow Chrome sign-in" toggle | Redundant without Google Services |

---

## New Tab Page Modifications

The NTP is reduced to logo + search bar only — everything else removed:

- Feed surface (replaced with simple FrameLayout wrapper)
- Tiles / most-visited
- Composeplate button
- Home modules / explore shelf
- Sign-in promo
- Identity disc

---

## Adding a new platform

**To block URLs:** Create `platform_rules/<name>_block_rules.cc`/`.h` with `kPrefixBlockRules[]` and/or `kRegexBlockRules[]` arrays. Add the file to `BUILD.gn` and the include chain in `shorts_reels_blocker.cc`.

**To suppress feed UI:** Create `platforms/<name>_rules.cc`/`.h` with CSS/JS payload constants and entries in `kRules[]`. Add to `BUILD.gn` and `content_injection_rules.cc`.

No logic changes are ever required — both systems are fully data-driven.

---

## Build

```bash
cd ~/chromium-android/checkout/src
export PATH="$HOME/chromium-android/depot_tools:$PATH"
gn args out/Default
```

**GN args:**
```
target_os = "android"
target_cpu = "arm64"
is_debug = false
is_component_build = false
symbol_level = 0
enable_incremental_javac = true
```

**Build and deploy:**
```bash
autoninja -C out/Default chrome_public_apk
out/Default/bin/chrome_public_apk install
```
