# Distraction Blocker Implementation

Full implementation from the Chromium fork at `/home/hari/chromium-android/`.

## Architecture (two independent layers)

### Layer 1 — Navigation Throttle + TabHelper (`navigation_policy/`)
Hard-blocks URLs before any network request. Also catches same-document SPA transitions via a WebContentsObserver.

| Platform   | Blocked paths                          | Allowed paths              |
|------------|----------------------------------------|----------------------------|
| YouTube    | `/shorts/*`                            | everything else            |
| Instagram  | `/reels/*`                             | `/<user>/reel/*` (profiles)|
| Facebook   | `/reels/*`, `/reel/*`, `/watch/*`      | profiles, groups, etc.     |
| Reddit     | `/reels/*`, `/r/*/s/*` (regex shares)  | `/r/<sub>/*`               |
| X          | `/i/reels/*`                           | profiles, search           |
| LinkedIn   | `/videos/reels/*`                      | profiles, jobs, search     |

**Block behaviour:** The throttle returns `BLOCK_REQUEST` with the block page HTML embedded directly as error page content (`ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT, html)`). This is the **preferred approach** — Chromium's navigation system handles history correctly (blocked URL never enters the session history), Back button goes to the page before the blocked URL. For same-document SPA transitions, the TabHelper navigates to `chrome://distraction-blocked` with `should_replace_current_entry=true` to replace the committed blocked URL.

Two rule tables:
- `kPrefixBlockRules[]` — simple path-prefix rules with component-boundary enforcement
- `kRegexBlockRules[]` — RE2 patterns for complex paths like Reddit `/r/\w+/s/\w+` share links

RE2 patterns are compiled once via function-local static Meyer's singleton:
```cpp
static const auto* kCompiledPatterns = [] {
  auto* patterns =
      new std::vector<std::unique_ptr<RE2>>(std::size(kRegexBlockRules));
  for (size_t i = 0; i < std::size(kRegexBlockRules); ++i)
    (*patterns)[i] = std::make_unique<RE2>(std::string(kRegexBlockRules[i].pattern));
  return patterns;
}();
```

### Layer 2 — Content Injection Manager (`content_injection/`)
Injects CSS or JS at DOMContentLoaded to suppress algorithmic feed UI on pages you're still allowed to visit.

| Domain        | Path          | What's removed                                    | Strategy    |
|---------------|---------------|---------------------------------------------------|-------------|
| instagram.com | `/`           | Stories tray                                      | CSS         |
| instagram.com | `/explore`    | Algorithmic grid + infinite scroll                | CSS + body overflow:hidden |
| facebook.com  | all           | Stories tray, Reels shelves in feed               | CSS         |
| youtube.com   | all           | Shorts shelf on home page                         | CSS (SVG fill selector) |
| reddit.com    | `/`           | Algorithmic home feed                             | CSS         |
| linkedin.com  | `/`, `/feed`  | Algorithmic feed stream                           | CSS         |
| x.com         | `/home`       | Timeline, nav bar (MutationObserver, SPA-safe)    | JS          |
| x.com         | `/explore`    | Explore timeline, tab bar (MutationObserver)      | JS          |
| tumblr.com    | `/dashboard/stuff_for_you` | Recommended posts (MutationObserver) | JS |

**CSS vs JS rule of thumb:**
- **CSS** — preferred: no execution overhead, applies before first paint, survives React re-renders, page JS cannot interfere
- **JS + MutationObserver** — only for heavy SPAs (X, Tumblr) that reconstruct the entire feed subtree on every route change

### Injection details
- CSS is wrapped in `<style>` element via JS; the CSS string is JSON-encoded for safe escaping
- JS runs in isolated world `ISOLATED_WORLD_ID_CONTENT_END` (page cannot tamper, injected code cannot pollute globals)
- All JS payloads are IIFEs to prevent cross-visit state

## Session-wide block counter

An `std::atomic<int>` in the blocker's anonymous namespace tracks blocks across the browser session:

- **Incremented** before each block (both throttle `BLOCK_REQUEST` and TabHelper SPA navigation)
- **Read** by the WebUI via `ShortsReelsBlockerThrottle::GetBlockCount()`
- **Displayed** as "blocked N times this session" embedded directly in the HTML via `base::StrCat`

For the throttle's `BLOCK_REQUEST` approach, the HTML is built per-request and passed as error page content. For the WebUI (`chrome://distraction-blocked`), the HTML is built dynamically in the `SetRequestFilter` callback.

⚠️ `loadTimeData.getString()` does NOT work with inline `SetRequestFilter` WebUIs — use `base::StrCat` to embed the count directly.

## C++ Design Patterns

### Rule tables in anonymous namespace
```cpp
namespace {
struct BlockRule {
  std::string_view registrable_domain;
  std::string_view path_prefix;
};
constexpr BlockRule kPrefixBlockRules[] = {
    {"youtube.com", "/shorts"},
    {"instagram.com", "/reels"},
    ...
};
struct RegexBlockRule {
  std::string_view registrable_domain;
  std::string_view pattern;
};
constexpr RegexBlockRule kRegexBlockRules[] = {
    {"reddit.com", R"(/r/[^/]+/s/[^/]+)"},
};
}  // namespace
```

### Path component boundary enforcement
`"/shortsfilm"` must NOT match prefix `"/shorts"`:
```cpp
static bool PathMatchesPrefix(std::string_view path, std::string_view prefix) {
  if (!base::StartsWith(path, prefix, base::CompareCase::SENSITIVE))
    return false;
  return path.size() == prefix.size() || path[prefix.size()] == '/';
}
```

### URL validation guard (both layers)
```cpp
if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
  return PROCEED;
```

### CheckURL must be public (TabHelper reuses it)
The TabHelper calls `CheckURL(url)` statically. It must be in the `public:` section of the throttle class, not `private:`.

### Multiplex at call site
Both TabHelper and ContentInjectionManager are attached at the same call site:
```cpp
// In CreateAndAddChromeThrottlesForNavigation():
registry.AddThrottle(ShortsReelsBlockerThrottle::CreateForNavigation(registry));

content::WebContents* wc = handle.GetWebContents();
if (wc) {
  ShortsReelsBlockerTabHelper::CreateForWebContents(wc);
  content_injection::ContentInjectionManager::CreateForWebContents(wc);
}
```

### ContentInjectionManager Header Pitfall
Forward-declare `RenderFrameHost` correctly:
```cpp
// ❌ BROKEN
namespace content::RenderFrameHost;

// ✅ CORRECT
namespace content {
class RenderFrameHost;
}
```

### `ExecuteJavaScriptInIsolatedWorld` arg order
The function signature is `(script, callback, world_id)`:
```cpp
// ✅ CORRECT
frame->ExecuteJavaScriptInIsolatedWorld(
    base::UTF8ToUTF16(script),
    /*callback=*/base::NullCallback(),
    content::ISOLATED_WORLD_ID_CONTENT_END);
```

### `isolated_world_ids.h` path
Include from `content/public/common/`, not `content/public/browser/`:
```cpp
#include "content/public/common/isolated_world_ids.h"   // ✅
// #include "content/public/browser/isolated_world_ids.h"  // ❌
```

## Compilation checklist

When adding new code to the distraction blocker, check these before building:

1. ✅ `base::RefCountedString` — add `#include "base/memory/ref_counted_memory.h"` for WebUI serving
2. ✅ `base::StrCat` (not `base::StringPrintf`) — embed dynamic values in raw strings
3. ✅ Raw string `R"(...)"` — use custom delimiter (`R"BLOCK(...)BLOCK"`) if HTML contains SVG data URIs with `)"`
4. ✅ `net::ERR_BLOCKED_BY_CLIENT` — add `#include "net/base/net_errors.h"` when using BLOCK_REQUEST + error_page_content
5. ✅ `content::GetUIThreadTaskRunner` — header is `content/public/browser/browser_thread.h`
6. ✅ `namespace content::RenderFrameHost;` — use proper class fwd decl
7. ✅ `CheckURL` — must be `public:` if TabHelper calls it
8. ✅ `isolated_world_ids.h` — path is `content/public/common/`
9. ✅ `ExecuteJavaScriptInIsolatedWorld` — args: `(script, callback, world_id)`
10. ✅ `GetNameForLogging()` — NOT const (virtual in base is non-const)
11. ✅ `#include "base/memory/ptr_util.h"` — needed for `base::WrapUnique`
12. ✅ BUILD.gn — use `source_set` per module, add dep to chrome/browser/BUILD.gn (never both inline sources AND source_set — duplicate symbols)

## File locations

- Patches: `/home/hari/chromium-android/patches/`
- Checkout source: `/home/hari/chromium-android/checkout/src/chrome/browser/navigation_policy/`
- Checkout source: `/home/hari/chromium-android/checkout/src/chrome/browser/content_injection/`
- Apply script: `/home/hari/chromium-android/patches/apply-patches.py` (also in skill at `scripts/apply-patches.py`)
- Integration guide: `/home/hari/chromium-android/patches/INTEGRATION.md`
- Architecture docs: `/home/hari/chromium-android/patches/README.md`

## How to add a new platform

### Block URLs (Layer 1)
1. Add a row to `kPrefixBlockRules[]` in `shorts_reels_blocker.cc`
2. For regex patterns (e.g. `/r/*/s/*`), add a row to `kRegexBlockRules[]`
3. Rebuild — no logic changes needed

### Suppress feed UI (Layer 2)
1. Add a CSS or JS payload constant in `content_injection_rules.cc`
2. Add a row to `kRules[]` with domain, path, type, and payload
3. Rebuild — no logic changes needed
