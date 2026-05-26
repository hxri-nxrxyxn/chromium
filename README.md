# Distraction Blocker — Chromium for Android
## Architecture overview

Two independent systems, each with a single responsibility.

---

### Layer 1 — Navigation Throttle + Tab Helper
**Files:** `chrome/browser/navigation_policy/`

Hard-blocks short-form video feed URLs **before any network request is made**.
Also catches same-document SPA transitions (pushState/replaceState) the
throttle never sees.

**Blocked URLs:**

| Platform   | Blocked paths                          | Allowed paths              |
|------------|----------------------------------------|----------------------------|
| YouTube    | `/shorts/*`                            | everything else            |
| Instagram  | `/reels/*`                             | `/<user>/reel/*` (profiles)|
| Facebook   | `/reels/*`, `/reel/*`, `/watch/*`      | profiles, groups, etc.     |
| Reddit     | `/reels/*`, `/r/*/s/*` (share links)   | `/r/<sub>/*`               |
| X          | `/i/reels/*`                           | profiles, search           |
| LinkedIn   | `/videos/reels/*`                      | profiles, jobs, search     |

---

### Layer 2 — Content Injection Manager
**Files:** `chrome/browser/content_injection/`

Injects CSS or JS at DOMContentLoaded to suppress algorithmic shelf/feed UI
on pages you're still allowed to visit.

**What gets removed per domain:**

| Domain        | Path          | What's removed                                    |
|---------------|---------------|---------------------------------------------------|
| instagram.com | `/`           | Stories tray                                      |
| instagram.com | `/explore`    | Algorithmic grid + infinite scroll                |
| facebook.com  | all           | Stories tray, Reels shelves in feed               |
| youtube.com   | all           | Shorts shelf on home page                         |
| reddit.com    | `/`           | Algorithmic home feed                             |
| linkedin.com  | `/`, `/feed`  | Algorithmic feed stream                           |
| x.com         | `/home`       | Timeline, nav bar (MutationObserver, SPA-safe)    |
| x.com         | `/explore`    | Explore timeline, tab bar (MutationObserver)      |
| tumblr.com    | `/dashboard/stuff_for_you` | Recommended posts (MutationObserver) |

---

### Adding a new platform

**To block a URL:** add one row to `kPrefixBlockRules[]` in
`shorts_reels_blocker.cc`. For patterns that can't be expressed as a prefix
(e.g. `/r/<subreddit>/s/<id>`), add a row to `kRegexBlockRules[]`.

**To suppress feed UI:** add a CSS or JS payload constant and one row to
`kRules[]` in `content_injection_rules.cc`.

No logic changes are ever required — both systems are fully data-driven.

---

### Design decisions

- **CSS over JS** wherever possible: no execution overhead, survives React
  re-renders, applies before first paint (no flash of unwanted content).
- **JS + MutationObserver** only for heavy SPAs (X, Tumblr) that reconstruct
  the entire feed subtree on every route change.
- **Isolated world** (`ISOLATED_WORLD_ID_CONTENT_END`): injected code cannot
  be tampered with by page JS, and cannot accidentally pollute page globals.
- **TabHelper attachment at call site** (not inside the throttle): keeps the
  two classes independently testable.
- **CheckURL reuse**: `ShortsReelsBlockerTabHelper::MaybeBlockURL` calls
  `ShortsReelsBlockerThrottle::CheckURL` directly — block logic lives in
  exactly one place.
