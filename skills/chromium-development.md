---
name: chromium-development
description: "Build Chromium for Android inside a Docker container — self-contained, one-command cleanup, no host pollution."
version: 2.0.0
author: Hermes Agent
license: MIT
platforms: [linux]
metadata:
  hermes:
    tags: [chromium, android, build, docker, depot_tools, gn]
    related_skills: []
---

# Chromium Android Build (Docker)

## Overview

Build Chromium for Android (`chrome_public_apk`) inside a Docker container with all tooling isolated. Everything lives in one directory (`~/chromium-android/`) — remove it with `rm -rf` to fully decommission.

## When to Use

- User wants to compile Chromium (or a fork) for Android
- User wants a clean, nukeable build environment
- User has limited RAM (<16GB) and needs `symbol_level` tuning to avoid OOM
- User wants to avoid scattering depot_tools, SDKs, and NDKs across the filesystem

## Directory Structure

```
~/chromium-android/
├── Dockerfile              # Ubuntu 22.04 + base tooling
├── docker-compose.yml      # bind mounts for source + depot_tools
├── Makefile                # setup → fetch → patches → deps → configure → build → unpatches → nuke
├── build-logs/             # tee'd build logs per session
├── depot_tools/            # cloned once, shared into container
├── checkout/               # bind mount: container /checkout → host ./checkout
│   ├── .gclient
│   └── src/
│       └── out/Default/    # build output + apks/
├── patches/                # custom source files (git-tracked)
│   ├── README.md           # architecture guide
│   ├── INTEGRATION.md      # exact lines to paste into Chrome files
│   ├── apply-patches.py    # idempotent copy + registration script
│   ├── .git/               # remote: github.com/hxri-nxrxyxn/chromium
    └── source-files/
        └── chrome/browser/
            ├── navigation_policy/      # ShortsReelsBlockerThrottle + TabHelper
            │   ├── BUILD.gn
            │   ├── shorts_reels_blocker.h
            │   └── shorts_reels_blocker.cc
            ├── content_injection/      # ContentInjectionManager (DOMContentLoaded)
            │   ├── BUILD.gn
            │   ├── content_injection_rules.h
            │   ├── content_injection_rules.cc
            │   ├── content_injection_manager.h
            │   └── content_injection_manager.cc
            └── ui/webui/distraction_blocked/  # chrome://distraction-blocked WebUI
                ├── distraction_blocked_ui.h
                └── distraction_blocked_ui.cc
```

## Workflow Steps

### Phase 0: Setup (one-time)

```bash
cd ~/chromium-android
make setup
```

Does:
1. `git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git`
2. Sets Git safe.directory for bind-mounted dirs
3. `docker compose build` — builds the image

### Logging Convention

For any long-running step (fetch, deps, build), `make` automatically logs output to `build-logs/<step>.log` via `tee -a`. Check progress with:

```bash
tail -f build-logs/fetch.log
```

The Makefile uses `LOG_DIR ?= build-logs` and `LOG_FILE ?= $(LOG_DIR)/<step>.log` — log files accumulate per session for debugging.

### Phase 1: Fetch source

```bash
make fetch
```

Runs `fetch --nohooks --no-history android` inside the container. Takes ~30 min on fast internet, downloads ~15-30GB.

**Flags:**
- `--nohooks` — skip hooks (runs separately in Phase 2)
- `--no-history` — shallow clone, saves disk space

### Phase 1b: Apply custom patches

```bash
make patches
```

Copies `.h`/`.cc` files from `patches/source-files/` into the checkout tree and applies registration changes to `chrome_content_browser_client_navigation_throttles.cc`. Idempotent — safe to re-run.

### Phase 2: Install build deps + hooks

```bash
make deps
```

Runs:
1. `./build/install-build-deps.sh` — installs all apt packages for building
2. `gclient runhooks` — downloads Android SDK/NDK and other binaries

### Phase 3: Configure

```bash
make configure
```

Writes `args.gn` to `checkout/src/out/Default/` (from the host) then runs `gn gen` inside the container.

**depot_tools bootstrap fix —** In Docker with `DEPOT_TOOLS_UPDATE=0`, the CIPD python never downloads, so `python3_bin_reldir.txt` is missing. Create it on the host (persists across container runs):

```bash
echo '../usr/bin' > ~/chromium-android/depot_tools/python3_bin_reldir.txt
```

This tells depot_tools to use the system Python 3 (`/usr/bin/python3`) instead of a CIPD-downloaded one. Without this, `gn` fails with `python3_bin_reldir.txt not found`. The Makefile's `configure` target assumes this file exists — create it during `make setup`.

**Recommended GN args (choose your profile):**

**Profile A — Development/Debugging (stack traces, fast links, RAM-friendly <16GB):**

```gn
target_os = "android"
target_cpu = "arm64"
symbol_level = 1                # Stack traces but not full DWARF
blink_symbol_level = 0          # No Blink symbols (huge savings)
v8_symbol_level = 0             # No V8 symbols (huge savings)
is_debug = false                # Release build
treat_warnings_as_errors = false
android_static_analysis = "off" # Required when using ninja directly (not autoninja)
```

**Profile B — Production/Performance (smallest APK, fastest runtime, no debug symbols):**

```gn
target_os = "android"
target_cpu = "arm64"
is_debug = false
symbol_level = 0                # No debug symbols — smaller APK, faster
dcheck_always_on = false        # No runtime assertions — better performance
is_official_build = true        # Full optimization: LTO, PGO, etc.
treat_warnings_as_errors = false
enable_rust = false             # Skip Rust sysroot cross-compile pitfalls
enable_profiling = false        # No profiling instrumentation
use_thin_lto = true             # ThinLTO for Android (smaller than full LTO)
android_static_analysis = "off" # Required when using ninja directly
```

Use Profile B when the user reports sluggish UI or crashes on the previous build. The `is_official_build=true` flag enables Clang LTO/PGO passes that produce a noticeably faster APK at the cost of longer build time (especially during the final link).

**Why these for Profile A:**
- `symbol_level=1` retains source-line info for crash debugging
- `blink_symbol_level=0` and `v8_symbol_level=0` save gigabytes of link-time RAM — critical on 15GB machines
- Without these, the linker (`lld`) can OOM on machines with <16GB RAM
- `is_debug=false` produces a release-optimized build

**Why these for Profile B:**
- `symbol_level=0` + `dcheck_always_on=false` — the most significant performance improvements over Profile A. Runtime assertions (`DCHECK`) add measurable overhead even in release builds.
- `is_official_build=true` — enables Clang LTO, PGO, and other optimizations normally gated for official releases. This is the single biggest performance boost but also the longest link phase.

### Phase 4: Build

```bash
make build
```

Runs `ninja -j4 -C out/Default chrome_public_apk`.

Output: `checkout/src/out/Default/apks/ChromePublic.apk`

### Monitoring build progress

Ninja's target count (e.g. `[13695/50245]`) can be misleading because compile, link, and code-gen targets all count differently. A more reliable progress indicator is the **`.o` file count**:

```bash
# Host-side check inside the running container:
docker exec <container-name> \
  sh -c "find /checkout/src/out/Default -name '*.o' -newer /checkout/src/out/Default/args.gn | wc -l"
```

This counts only files compiled since the last `gn gen`. Total targets is a rough upper bound.

#### Cron-based automated progress reporting

For long builds that span hours, set up a cron job that checks `.o` file count automatically. The skill ships a ready-to-use script at `scripts/chromium-progress.sh`:

```bash
hermes cron create \
  --name "Chromium Build Progress" \
  --prompt "Check chromium build progress and report verbatim." \
  --script chromium-progress.sh \
  --no-agent \
  --schedule "every 15m"
```

This uses `no_agent=true` (zero LLM cost) — the script's stdout is delivered verbatim each tick. The script execs into the running container to count `.o` files and never invokes `ninja` (no lock conflicts with the running build). Works reliably where reading the build log from the host fails (pipeline buffering masks live progress).

**⚠️ Container naming pitfall:** The script matches containers via `docker ps --filter name=chromium`. If you start the build with `--name chromium-builder`, the filter still finds it via substring match. If the cron reports "container not running" unexpectedly, check `docker ps | grep chromium` to verify the actual container name.

**Log fallback:** If the container auto-removed (`--rm`) and only the log file remains, the script falls back to extracting the last `[N/M]` pattern from `build_v2.log` and reports finishing progress.

**Verifying the build is actively compiling** (not stuck):

```bash
# Check if clang++ processes are running with recent activity:
docker exec <container-name> ps aux | grep clang++ | grep -v grep | grep -v '/bin/sh -c'
# Expect 2-4 processes at 100% CPU each

# Check timestamps of most recently compiled .o files (epoch timestamps):
docker exec <container-name> \
  sh -c "find /checkout/src/out/Default -name '*.o' -newer /checkout/src/out/Default/args.gn -printf '%T@ %p\\n' 2>/dev/null | sort -rn | head -3"
# Recent timestamps = actively compiling
```

**Thermal monitoring during long builds:**

```bash
cat /sys/class/thermal/thermal_zone*/temp
```
Values are in millidegrees Celsius. For a 35W TDP chip, 60-70°C sustained under `-j4` is normal and safe.

### Build log caveat

The Makefile uses `tee -a` for logging, which creates a pipeline that masks ninja's exit code. The log file may stop showing progress even though the build is still running (buffered stdout). Trust the `.o` file count and the active clang++ processes over the log tail.

### Phase 5: Push patches to GitHub

After every successful build, commit and push the patches directory to the repo:

```bash
cd ~/chromium-android/patches
git add -A && git commit -m "build $(date +%Y-%m-%d): <brief description>"
git push
```

Patches repo: **https://github.com/hxri-nxrxyxn/chromium** (branch `main`)

Only the `patches/` directory is tracked — not the Chromium source or build artifacts.

### Install to device

```bash
make install
```

Requires a connected Android device with USB debugging enabled.

## Custom Patches (Navigation Throttles, URL Blocking, etc.)

The build supports injecting custom `.h`/`.cc` files and patching existing Chromium source via a `patches/` directory:

```
~/chromium-android/
└── patches/
    ├── apply-patches.py            # copies files + patching logic
    └── source-files/               # .h/.cc files injected verbatim
        └── chrome/browser/navigation_policy/
            ├── my_blocker.h
            └── my_blocker.cc
```

### Content Injection Manager — CSS/JS at DOMContentLoaded

For pages you don't want to block outright but want to **suppress specific UI elements** (algorithmic feeds, stories trays, reels shelves), use a `WebContentsObserver` that injects CSS/JS at `DOMContentLoaded`.

**When to use CSS vs JS:**
- **CSS** — preferred for most cases. No execution overhead, applies before first paint (no flash), survives React re-renders, selectors can be pagelet-data-attribute-based. Write as a raw string constant in an anonymous namespace.
- **JS + MutationObserver** — only for heavy SPAs (X/Twitter, Tumblr) that reconstruct the entire feed subtree on every route change, clobbering CSS-only injections.

**Rule table pattern** — same `constexpr` struct array approach as the throttle:

```cpp
struct InjectionRule {
  std::string_view registrable_domain;  // GURL::DomainIs()
  std::string_view path_prefix;         // "" = all paths on domain
  InjectionType type;                    // kCSS or kJavaScript
  std::string_view payload;             // raw CSS or JS string
};

constexpr InjectionRule kRules[] = {
    {"instagram.com", "/",        InjectionType::kCSS,        kInstagramHomeCSS},
    {"instagram.com", "/explore", InjectionType::kCSS,        kInstagramExploreCSS},
    {"x.com",         "/home",    InjectionType::kJavaScript, kXHomeJS},
    ...
};
```

**Manager class** — a `WebContentsObserver` + `WebContentsUserData<T>` (same CRTP pattern as the TabHelper) that listens for `DOMContentLoaded`:

```cpp
// content_injection_manager.h
class ContentInjectionManager final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContentInjectionManager> {
 public:
  ~ContentInjectionManager() override;
  ContentInjectionManager(const ContentInjectionManager&) = delete;
  ContentInjectionManager& operator=(const ContentInjectionManager&) = delete;
 private:
  explicit ContentInjectionManager(content::WebContents* wc);
  friend class content::WebContentsUserData<ContentInjectionManager>;
  void DOMContentLoaded(content::RenderFrameHost* rfh) override;
  void RunMatchingRules(content::RenderFrameHost* frame, const GURL& url);
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
```

**Injection implementation:**
- CSS is wrapped in a `<style>` element via JavaScript (`document.createElement('style')` + `document.head.appendChild`). The CSS string is JSON-encoded before insertion to safely escape quotes, newlines, etc.
- JS is executed as-is in an **isolated world** (`ISOLATED_WORLD_ID_CONTENT_END`). This means page JS cannot tamper with injected code, and injected code cannot accidentally pollute page globals.
- All JS payloads should be IIFEs to prevent cross-visit pollution.

```cpp
// In RunMatchingRules — CSS path:
std::string json_css;
base::JSONWriter::Write(base::Value(std::string(rule.payload)), &json_css);
script = "(function(){"
         "var s=document.createElement('style');"
         "s.textContent=" + json_css + ";"
         "document.head.appendChild(s);"
         "})();";
frame->ExecuteJavaScriptInIsolatedWorld(
    base::UTF8ToUTF16(script),
    /*callback=*/base::NullCallback(),
    content::ISOLATED_WORLD_ID_CONTENT_END);
```

**Attaching both the throttle's TabHelper AND the injection manager at the call site** (in `chrome_content_browser_client_navigation_throttles.cc`):

```cpp
registry.AddThrottle(
    ShortsReelsBlockerThrottle::CreateForNavigation(registry));

content::WebContents* web_contents = handle.GetWebContents();
if (web_contents) {
  ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
  content_injection::ContentInjectionManager::CreateForWebContents(
      web_contents);
}
```

Both `CreateForWebContents()` calls are idempotent — the `WebContentsUserData<T>` CRTP returns the existing instance if one already exists. This pattern keeps the throttle, TabHelper, and injection manager independently testable.

**BUILD.gn** for the injection module (self-contained `source_set`):

```gn
source_set("content_injection") {
  sources = [
    "content_injection_manager.cc",
    "content_injection_manager.h",
    "content_injection_rules.cc",
    "content_injection_rules.h",
  ]
  deps = [
    "//base",
    "//content/public/browser",
    "//url",
  ]
}
```

Then add the dep to `chrome/browser/BUILD.gn`:
```gn
deps += [
  "//chrome/browser/content_injection:content_injection",
  "//chrome/browser/navigation_policy:shorts_reels_blocker",
]
```

**Example CSS payloads** that can live in `content_injection_rules.cc`:

```cpp
// instagram.com/ — hide Stories tray; main feed is intentional
constexpr std::string_view kInstagramHomeCSS = R"CSS(
  [data-pagelet="story_tray"] { display: none !important; }
)CSS";

// youtube.com — hide the Shorts shelf on the home page (via red icon SVG fill)
constexpr std::string_view kYoutubeShortsShelfCSS = R"CSS(
  ytm-rich-section-renderer:has(span.yt-icon-shape svg path[fill="#f03"]) {
    display: none !important;
  }
)CSS";
```

**Example JS payload** for heavy SPAs (MutationObserver, IIFE):

```js
(function() {
  'use strict';
  const SELECTORS = [
    'div[aria-label="Timeline: Your Home Timeline"]',
    'nav[role="navigation"][aria-live="polite"]',
  ].join(',');

  const removeMatching = () => {
    document.querySelectorAll(SELECTORS).forEach(el => el.remove());
  };

  removeMatching();
  new MutationObserver(removeMatching).observe(document.body, {
    childList: true, subtree: true,
  });
})();
```

### Adding a NavigationThrottle (e.g. block YouTube Shorts / Instagram Reels)

Use a **two-layer design** — a `NavigationThrottle` for initial/redirect navigations, plus a `WebContentsObserver` (attached at the call site) for same-document SPA transitions like pushState. The classes remain independently testable — the throttle never attaches the observer; the call site does.

1. **Create the throttle header** (`chrome/browser/navigation_policy/my_blocker.h`):
   ```cpp
   #ifndef CHROME_BROWSER_NAVIGATION_POLICY_MY_BLOCKER_H_
   #define CHROME_BROWSER_NAVIGATION_POLICY_MY_BLOCKER_H_

   #include <string_view>

   #include "content/public/browser/navigation_throttle.h"
   #include "content/public/browser/web_contents_observer.h"
   #include "content/public/browser/web_contents_user_data.h"

   class BlockListThrottle final : public content::NavigationThrottle {
    public:
     static std::unique_ptr<BlockListThrottle> CreateForNavigation(
         content::NavigationThrottleRegistry& registry);
     ~BlockListThrottle() override;

     ThrottleCheckResult WillStartRequest() override;
     ThrottleCheckResult WillRedirectRequest() override;
     const char* GetNameForLogging() override;  // NOT const

    private:
     explicit BlockListThrottle(
         content::NavigationThrottleRegistry& registry);

     [[nodiscard]] static ThrottleCheckResult CheckURL(const GURL& url);
     [[nodiscard]] static bool PathMatchesPrefix(std::string_view path,
                                                 std::string_view prefix);
   };

   class BlockListTabHelper final
       : public content::WebContentsObserver,
         public content::WebContentsUserData<BlockListTabHelper> {
    public:
     ~BlockListTabHelper() override;
     BlockListTabHelper(const BlockListTabHelper&) = delete;
     BlockListTabHelper& operator=(const BlockListTabHelper&) = delete;

    private:
     explicit BlockListTabHelper(content::WebContents* web_contents);
     friend class content::WebContentsUserData<BlockListTabHelper>;

     void DidFinishNavigation(
         content::NavigationHandle* navigation_handle) override;
     void MaybeBlockURL(const GURL& url);
     WEB_CONTENTS_USER_DATA_KEY_DECL();
   };

   #endif
   ```
   ⚠️ `GetNameForLogging()` is **NOT const** — the base class declares it as `virtual const char* GetNameForLogging() = 0;` Adding `const` causes a compile error.
   ⚠️ `final` is important — these classes aren't designed for inheritance. `WebContentsUserData` subclasses should also be `final` since the CRTP pattern makes derivation error-prone.
   ⚠️ `url/gurl.h` is NOT needed explicitly — it's pulled in transitively. Include `<string_view>` instead for `PathMatchesPrefix`.

2. **Implement the throttle + tab helper** (`chrome/browser/navigation_policy/my_blocker.cc`):
   ```cpp
   #include "chrome/browser/navigation_policy/my_blocker.h"

   #include <string_view>

   #include "base/memory/ptr_util.h"
   #include "base/strings/string_util.h"
   #include "content/public/browser/navigation_controller.h"
   #include "content/public/browser/navigation_handle.h"
   #include "content/public/browser/navigation_throttle_registry.h"
   #include "content/public/browser/web_contents.h"
   #include "ui/base/page_transition_types.h"
   #include "url/gurl.h"

   namespace {

   struct BlockRule {
     std::string_view registrable_domain;
     std::string_view path_prefix;
   };

   constexpr BlockRule kBlockRules[] = {
       {"youtube.com", "/shorts"},
       {"instagram.com", "/reels"},
   };

   }  // namespace

   // --- Throttle ---

   // static
   std::unique_ptr<BlockListThrottle>
   BlockListThrottle::CreateForNavigation(
       content::NavigationThrottleRegistry& registry) {
     return base::WrapUnique(new BlockListThrottle(registry));
   }

   BlockListThrottle::BlockListThrottle(
       content::NavigationThrottleRegistry& registry)
       : content::NavigationThrottle(registry) {}

   BlockListThrottle::~BlockListThrottle() = default;

   ThrottleCheckResult BlockListThrottle::WillStartRequest() {
     return CheckURL(navigation_handle()->GetURL());
   }

   ThrottleCheckResult BlockListThrottle::WillRedirectRequest() {
     return CheckURL(navigation_handle()->GetURL());
   }

   const char* BlockListThrottle::GetNameForLogging() {
     return "BlockListThrottle";
   }

   // static
   ThrottleCheckResult BlockListThrottle::CheckURL(const GURL& url) {
     if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
       return PROCEED;

     const std::string lower_path = base::ToLowerASCII(url.path());
     for (const auto& rule : kBlockRules) {
       if (url.DomainIs(rule.registrable_domain) &&
           PathMatchesPrefix(lower_path, rule.path_prefix)) {
         return BLOCK_REQUEST;
       }
     }
     return PROCEED;
   }

   // static
   bool BlockListThrottle::PathMatchesPrefix(std::string_view path,
                                              std::string_view prefix) {
     if (!base::StartsWith(path, prefix, base::CompareCase::SENSITIVE))
       return false;
     // "/shortsfilm" must NOT match prefix "/shorts".
     return path.size() == prefix.size() || path[prefix.size()] == '/';
   }

   // --- TabHelper ---

   WEB_CONTENTS_USER_DATA_KEY_IMPL(BlockListTabHelper);

   BlockListTabHelper::BlockListTabHelper(content::WebContents* wc)
       : content::WebContentsObserver(wc),
         content::WebContentsUserData<BlockListTabHelper>(*wc) {}

   BlockListTabHelper::~BlockListTabHelper() = default;

   void BlockListTabHelper::DidFinishNavigation(
       content::NavigationHandle* handle) {
     if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted())
       return;
     MaybeBlockURL(handle->GetURL());
   }

   void BlockListTabHelper::MaybeBlockURL(const GURL& url) {
     if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
       return;

     const std::string lower_path = base::ToLowerASCII(url.path());
     bool blocked = false;
     for (const auto& rule : kBlockRules) {
       if (url.DomainIs(rule.registrable_domain) &&
           BlockListThrottle::PathMatchesPrefix(lower_path, rule.path_prefix)) {
         blocked = true;
         break;
       }
     }
     if (!blocked) return;

     auto& ctrl = web_contents()->GetController();
     if (ctrl.CanGoBack())
       ctrl.GoBack();
     else
       ctrl.LoadURL(GURL("about:blank"), content::Referrer(),
                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL, {});
   }
   ```
   Key patterns:
   - `base::WrapUnique` (not `std::make_unique`) — Chromium convention for private constructors
   - Anonymous namespace + `constexpr` struct array for config — easy to extend
   - URL validation (`is_valid` + `SchemeIsHTTPOrHTTPS`) before any inspection
   - `PathMatchesPrefix` prevents false matches on longer words
   - Both throttle and TabHelper reuse the same `kBlockRules` and `PathMatchesPrefix`

3. **Register both** in `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`:
   - Add `#include "chrome/browser/navigation_policy/my_blocker.h"`
   - In `CreateAndAddChromeThrottlesForNavigation()`:
     ```cpp
     registry.AddThrottle(
         BlockListThrottle::CreateForNavigation(registry));
     BlockListTabHelper::CreateForWebContents(
         handle.GetWebContents());
     ```
   - The function receives `content::NavigationThrottleRegistry& registry` and gets the handle via `content::NavigationHandle& handle = registry.GetNavigationHandle();`.
   - `CreateForWebContents()` is idempotent — safe to call every navigation.

### ⚠️ SPA pushState Interception (YouTube Shorts Tab, Instagram Reels Tab)

**Problem:** `NavigationThrottle` only fires on actual page navigations (link clicks, typed URLs, redirects). SPA sites like YouTube load content via `history.pushState()` + fetch API calls — the URL changes in the address bar but **no NavigationThrottle fires**. The Shorts tab works despite the throttle because it's a client-side transition, not a navigation.

**Two approaches were attempted — one BROKEN, one WORKING.**

#### ❌ BROKEN: JS Injection via WillProcessResponse

Injecting JavaScript via `RenderFrameHost::ExecuteJavaScript()` inside `WillProcessResponse()` **crashes the renderer**. Do NOT attempt this. Full details in `references/spa-pushstate-interception.md`.

#### ✅ WORKING: WebContentsObserver + WebContentsUserData<T> (attached at call site)

Create a `WebContentsObserver` subclass, persist it on the `WebContents` via the `WebContentsUserData<T>` CRTP template, and intercept `DidFinishNavigation` (which fires for pushState transitions too).

The TabHelper must be **attached at the registration call site** (in `chrome_content_browser_client_navigation_throttles.cc`), NOT inside the throttle. This keeps the two classes independently testable and avoids the problem of `navigation_handle()` being null during factory construction.

**Header pattern** — multiply-inherit `content::WebContentsObserver` + `content::WebContentsUserData<YourTabHelper>`. Declare with `WEB_CONTENTS_USER_DATA_KEY_DECL()`. See the "Adding a NavigationThrottle" section above for a full example with `final`, `= delete` copy ops, and `std::string_view`.

**Constructor** must initialize both bases:
```cpp
YourTabHelper(content::WebContents* wc)
    : content::WebContentsObserver(wc),
      content::WebContentsUserData<YourTabHelper>(*wc) {}
```

**`DidFinishNavigation`** fires for ALL committed main-frame URL changes including pushState:
```cpp
void DidFinishNavigation(NavigationHandle* handle) override {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted())
    return;
  MaybeBlockURL(handle->GetURL());
}
```

**Registration** (at the call site, not inside the throttle):
```cpp
// In CreateAndAddChromeThrottlesForNavigation():
registry.AddThrottle(
    YourThrottle::CreateForNavigation(registry));
YourTabHelper::CreateForWebContents(handle.GetWebContents());
```
`CreateForWebContents()` is idempotent — safe to call every navigation because `WebContentsUserData::CreateForWebContents()` returns the existing instance if one already exists.

**`.cc` requires:** `#include "content/public/browser/navigation_controller.h"`, `#include "ui/base/page_transition_types.h"`, and `WEB_CONTENTS_USER_DATA_KEY_IMPL(YourTabHelper)`.

### `CheckURL` must be public if TabHelper reuses it

When the TabHelper calls `CheckURL(url)` statically to reuse the throttle's block logic, `CheckURL` must be in the `public:` section of the throttle class. A `private:` CheckURL causes:

```
error: 'CheckURL' is a private member of 'ShortsReelsBlockerThrottle'
```

Move `CheckURL` and `PathMatchesPrefix` to `public:` if the TabHelper (or any other class) needs to call them.

### `ExecuteJavaScriptInIsolatedWorld` argument order

The function signature is `(script, callback, world_id)`, NOT `(script, world_id, callback)`:

```cpp
// ✅ CORRECT
frame->ExecuteJavaScriptInIsolatedWorld(
    base::UTF8ToUTF16(script),
    /*callback=*/base::NullCallback(),             // 2nd: callback
    content::ISOLATED_WORLD_ID_CONTENT_END);        // 3rd: world_id

// ❌ BROKEN — ISOLATED_WORLD_ID_CONTENT_END is an int, gets passed as callback
frame->ExecuteJavaScriptInIsolatedWorld(
    base::UTF8ToUTF16(script),
    content::ISOLATED_WORLD_ID_CONTENT_END,         // 2nd: ❌ this is the world_id
    /*callback=*/base::NullCallback());             // 3rd: ❌ this is too many args
```

Symptom: `error: no matching constructor for initialization of 'base::OnceCallback<void (base::Value)>'` — the compiler tries to construct a callback from the world_id integer.

### `isolated_world_ids.h` is in `content/public/common/`, not `browser/`

The `ISOLATED_WORLD_ID_GLOBAL`, `ISOLATED_WORLD_ID_CONTENT_END` constants live at:

```cpp
#include "content/public/common/isolated_world_ids.h"   // ✅ correct
// #include "content/public/browser/isolated_world_ids.h"  // ❌ wrong path
```

Symptom: `fatal error: 'content/public/browser/isolated_world_ids.h' file not found`.

### `content::GetUIThreadTaskRunner` header

`content::GetUIThreadTaskRunner({})` requires:
```cpp
#include "content/public/browser/browser_thread.h"   // ✅ correct
// #include "content/public/browser/browser_task_traits.h"  // ❌ wrong
```

Using `browser_task_traits.h` gives: `error: no member named 'GetUIThreadTaskRunner' in namespace 'content'; did you mean 'base::SingleThreadTaskRunner'?`

### `net::ERR_BLOCKED_BY_CLIENT` requires `net/base/net_errors.h`

When returning `ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT, ...)` from a throttle, the `net::ERR_BLOCKED_BY_CLIENT` constant is in:

```cpp
#include "net/base/net_errors.h"   // ✅ for net::ERR_BLOCKED_BY_CLIENT
```

This include is NOT pulled in transitively by `content/public/browser/navigation_throttle.h`. Missing it causes: `use of undeclared identifier 'net::ERR_BLOCKED_BY_CLIENT'`.

### C++ raw string `R"(...)` prematurely closed by `)"` in data URIs

When embedding SVG/CSS data URIs inside a C++ raw string (`R"(...)`), the `)"` sequence at the end of a data URI (`%3E")`) terminates the raw string early. The compiler then sees the rest of the HTML as code, producing `expected unqualified-id` at the raw string's closing line.

**Fix:** Use a custom delimiter that doesn't appear in the content:

```cpp
// ❌ BROKEN — data URI's )" matches R"(...)'s closing delimiter
constexpr char kCSS[] = R"CSS(
  .icon { background-image: url("data:image/svg+xml,...%3E"); }
)CSS";  // ^^ %3E") matches )" and ends the string here!

// ✅ WORKS — custom delimiter
constexpr char kCSS[] = R"CSS(
  .icon { background-image: url("data:image/svg+xml,...%3E"); }
)CSS";  // )CSS" only matches )CSS", ignores )"
```

Common safe delimiters: `R"CSS(`, `R"HTML(`, `R"BLOCK(`, `R"JS(`.

### `base::StringPrintf` + raw string literals = compile error

Do NOT use `base::StringPrintf` with `R"HTML(..."...%s...")HTML"` — the compile-time format spec checker trips on `%s`:

```
error: 'FormatSpecTemplate<void>' is unavailable: Format specified does not match the arguments passed.
```

**Fix:** Use `base::StrCat` and split at the substitution point:
```cpp
// ❌ BROKEN — format spec checking error
return base::StringPrintf(R"HTML(<div class="error-code">%s</div>)HTML", val.c_str());

// ✅ WORKS — join at the seam
#include "base/strings/strcat.h"
return base::StrCat({R"HTML(<div class="error-code">)HTML", val, R"HTML(</div>)HTML"});
```

### `namespace content::RenderFrameHost;` is INVALID for forward declarations

This C++17 nested-namespace syntax does NOT work for forward-declaring a class. It declares `RenderFrameHost` as a namespace, not a class, causing `expected '{'` at compile time:

```cpp
// ❌ BROKEN — declares RenderFrameHost as a namespace
namespace content::RenderFrameHost;

// ✅ CORRECT — proper class forward declaration
namespace content {
class RenderFrameHost;
}
```

Symptom: `error: expected '{'` on the `namespace content::RenderFrameHost;` line.

**⚠️ `base::WrapUnique` requires `#include "base/memory/ptr_util.h"`**

When using `base::WrapUnique(new ...)` to create throttles with private constructors, the include is needed explicitly. It is often pulled in transitively by `content/public/browser/navigation_throttle.h`, but on some toolchains the transitive include may not be present. Always add the explicit include to be safe.

Full working implementation is in the "Adding a NavigationThrottle" section above.

### ⚠️ CRITICAL: BUILD.gn source registration (source_set approach)

New `.cc` files must be **registered in the build system** or the linker silently fails with `undefined symbol`. The recommended way is a **self-contained `source_set`** in the module's own BUILD.gn:

```gn
# chrome/browser/navigation_policy/BUILD.gn
source_set("shorts_reels_blocker") {
  sources = [
    "shorts_reels_blocker.cc",
    "shorts_reels_blocker.h",
  ]
  deps = [
    "//base",
    "//content/public/browser",
    "//third_party/re2",    # if using RE2 for regex rules
    "//ui/base",
    "//url",
  ]
}
```

Then add the dep to `chrome/browser/BUILD.gn`:
```gn
deps += [
  "//chrome/browser/navigation_policy:shorts_reels_blocker",
]
```

**Alternative: inline sources** (simpler for one-off files, but risks duplicate symbols if a source_set also lists the same file):
```gn
# In chrome/browser/BUILD.gn's core target:
sources = [
  ...
  "navigation_policy/shorts_reels_blocker.cc",
  ...
]
deps = [
  ...
  "//third_party/re2",
  ...
]
```

**Symptom of missing BUILD.gn entry:**
```
ld.lld: error: undefined symbol: MyBlockerThrottle::MaybeCreateThrottleFor(
    content::NavigationThrottleRegistry&)
>>> referenced by ...
```

The `.o` file for your throttle is **never created** — only the registration code exists. The linker can't find the implementation.

### Patch script should handle BUILD.gn too (source_set deps)

Your `apply-patches.py` should patch BUILD.gn with source_set deps AND the registration file. Same pattern — guard with `in` check for idempotency:

```python
# In apply-patches.py — after file copy step:

# Patch the throttles file (includes + registration)
throttles_file = os.path.join(
    chromium_src, "chrome/browser/chrome_content_browser_client_navigation_throttles.cc")
with open(throttles_file) as f:
    content = f.read()

modified = False

# Add includes for both modules
includes = [
    '#include "chrome/browser/content_injection/content_injection_manager.h"',
    '#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"',
]
anchor = '#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"'
for inc in includes:
    if inc not in content:
        content = content.replace(anchor, anchor + "\n" + inc)
        modified = True

# Add throttle + tab-helper + injection-manager registration
if "ShortsReelsBlockerThrottle::CreateForNavigation" not in content:
    anchor = "page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);"
    insert = (
        "    // Block short-form video feed URLs.\n"
        "    registry.AddThrottle(\n"
        "        ShortsReelsBlockerThrottle::CreateForNavigation(registry));\n"
        "    content::WebContents* wc = handle.GetWebContents();\n"
        "    if (wc) {\n"
        "      ShortsReelsBlockerTabHelper::CreateForWebContents(wc);\n"
        "      content_injection::ContentInjectionManager::CreateForWebContents(wc);\n"
        "    }\n"
    )
    content = content.replace(anchor, anchor + "\n" + insert)
    modified = True

if modified:
    with open(throttles_file, "w") as f:
        f.write(content)

# Patch BUILD.gn with source_set deps (not inline sources)
build_gn = os.path.join(chromium_src, "chrome/browser/BUILD.gn")
with open(build_gn) as f:
    gn_content = f.read()

gn_modified = False
deps_to_add = [
    '"//chrome/browser/navigation_policy:shorts_reels_blocker"',
    '"//chrome/browser/content_injection:content_injection"',
]
anchor = '"//chrome/browser/navigation_predictor",'
insert = "\n    " + ",\n    ".join(deps_to_add) + ","
for dep in deps_to_add:
    if dep not in gn_content:
        gn_content = gn_content.replace(anchor, anchor + insert)
        gn_modified = True
        break

if gn_modified:
    with open(build_gn, "w") as f:
        f.write(gn_content)
```

### Patch script idempotency

Use a Python script (`apply-patches.py`) instead of sed for reliability:
- Guard all inserts with `in` checks on file content so re-runs don't double-patch
- Copy source files with `shutil.copy2` (overwrite is fine, content should be identical)
- Check for the new `CreateForNavigation` method name to detect the registration for idempotency (not the old `MaybeCreateThrottleFor`)
- Properly handle special characters (`//`, `::`) that break sed

### Reverting patches

```bash
make unpatches
```

Does: `git checkout -- .` to revert tracked files, then `rm -f` any new `.h`/`.cc` files added by patches.

## Creating a Custom WebUI Page (`chrome://`)

When you need a block screen, interstitial, or settings page rendered in Chromium, add a **WebUI controller** that serves HTML/CSS/JS via `WebUIDataSource`.

Two serving approaches exist:
- **Inline strings** (our default) — embed HTML/CSS/JS as `constexpr` literals in the `.cc` file, serve via `SetRequestFilter`. Self-contained, no GRIT changes needed.
- **GRIT resources** — use `.grd` files with resource IDs. Standard for complex pages but requires modifying the build system.

For native-looking interstitial pages, copy CSS variables and layout from `references/interstitial-css-variables.md` (derived from Chromium's `interstitial_core.css`, `interstitial_common.css`, and `neterror.css`).

### Registration pattern

| Step | File | Action |
|------|------|--------|
| 1 | `chrome/common/webui_url_constants.h` | Add `kChromeUIYourHost[]` constant |
| 2 | `chrome/browser/ui/webui/your_page/your_page_ui.h` | Create controller class + `DefaultWebUIConfig<YourPageUI>` config class |
| 3 | `chrome/browser/ui/webui/your_page/your_page_ui.cc` | Implement: create `WebUIDataSource`, add strings, set request filter with inline HTML |
| 4 | `chrome/browser/ui/webui/chrome_web_ui_configs.cc` | Add `#include` + `map.AddWebUIConfig(...)` call |
| 5 | `chrome/browser/ui/BUILD.gn` | Add `"webui/your_page/your_page_ui.cc"` and `".h"` to sources |
| 6 | `patches/source-files/` | Mirror new files; update `apply-patches.py` and `INTEGRATION.md` |

### Frontend pattern

For GRIT-based WebUIs with `UseStringsJs()`, Lit + `loadTimeData` is the standard pattern:

```html
<script type="module">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
const count = loadTimeData.getString('distractionBlockCountText');
</script>
```

Dynamic strings are set on the C++ side via `source->AddString("key", value)` before the request filter.

⚠️ **`loadTimeData.getString()` DOES NOT WORK with `SetRequestFilter` + inline strings.** The `strings.m.js` endpoint is never generated when `UseStringsJs()` is not called. For inline-string WebUIs, embed values directly in the HTML via `base::StrCat` in the request filter callback (see "Wiring a live block counter" section above).

### ⚠️ `base::RefCountedString` requires explicit include

When using `SetRequestFilter` with `base::MakeRefCounted<base::RefCountedString>` to serve inline HTML, the file MUST include:

```cpp
#include "base/memory/ref_counted_memory.h"
```

This is **not** pulled in transitively by `web_ui_data_source.h`. Missing it causes:

```
error: no member named 'RefCountedString' in namespace 'base'
```

Always add this include to the WebUI controller `.cc` file.

### Wiring a live block counter into the WebUI

For a session-wide block counter that the WebUI page reads:

1. **Atomic counter** in the blocker `.cc` (anonymous namespace):
```cpp
namespace {
std::atomic<int> g_block_count{0};
}  // namespace
```

2. **Counter accessors** on the throttle class (static, public):
```cpp
// In header:
static void NavigateToBlockPage(content::WebContents* web_contents);
static int GetBlockCount();

// In .cc:
void ShortsReelsBlockerThrottle::NavigateToBlockPage(
    content::WebContents* web_contents) {
  g_block_count.fetch_add(1, std::memory_order_relaxed);
  // ...deferred LoadURLWithParams...
}

int ShortsReelsBlockerThrottle::GetBlockCount() {
  return g_block_count.load(std::memory_order_relaxed);
}
```

3. **⚠️ CRITICAL: `loadTimeData.getString()` does NOT work with `SetRequestFilter`**

When serving inline HTML via `SetRequestFilter` (no GRIT, no `UseStringsJs()`), the `loadTimeData` module is never populated because `strings.m.js` is never generated. The `source->AddString("key", value)` call stores the data, but there's no JS `strings.m.js` endpoint to serve it to the browser.

**The working approach: build the HTML dynamically in the request filter callback with the value embedded directly:**

```cpp
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"

// Build HTML with the count embedded (no JS loadTimeData dependency):
std::string BuildPageHTML(int block_count) {
  std::string counter_text = base::StringPrintf(
      "blocked %d time%s this session",
      block_count, block_count == 1 ? "" : "s");

  // Use base::StrCat to embed values — NOT base::StringPrintf with %s,
  // which triggers compile-time format spec checking errors on raw strings.
  return base::StrCat({R"HTML(
    <!doctype html><html><head>...
    <div class="error-code">)HTML",
    counter_text,
    R"HTML(</div>
    ...</body></html>)HTML"});
}

void CreateAndAddHTMLSource(Profile* profile) {
  auto* source = content::WebUIDataSource::CreateAndAdd(...);

  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        return path.empty() || path == "index.html";
      }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            int count = ShortsReelsBlockerThrottle::GetBlockCount();
            std::string html = BuildPageHTML(count);
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::move(html)));
          }));
}
```

**Key rules for inline HTML + dynamic data:**
- Build the entire HTML string per-request with values embedded
- Use `base::StrCat` (not `base::StringPrintf` with `%s`) to avoid compile-time format spec checking errors on raw string literals
- Pass `MakeRefCounted<base::RefCountedString>` with the complete HTML — this is what `GotDataCallback` expects
- Requires `#include "base/memory/ref_counted_memory.h"`

### Returning a custom block page (BLOCK_REQUEST + inline HTML) — PREFERRED

**This is the preferred approach** for showing a block/interstitial page when a navigation throttle intercepts a URL. It avoids the history corruption issues with the PostTask approach (see below).

Instead of cancelling and navigating to a WebUI page separately, return `BLOCK_REQUEST` with the full block page HTML embedded as error page content:

```cpp
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"

std::string BuildBlockPageHTML(int count) {
  std::string counter_text = base::StringPrintf(
      "blocked %d time%s this session",
      count, count == 1 ? "" : "s");

  // ⚠️ Use custom raw string delimiter (R"BLOCK(...)BLOCK") when the
  // HTML contains data: URIs that may include )" in SVG/CSS content.
  // R"(...) is terminated by the first )" sequence.
  return base::StrCat({R"BLOCK(
    <!doctype html><html><head>...
    <div class="error-code">)BLOCK",
    counter_text,
    R"BLOCK(</div>
    ...</body></html>)BLOCK"});
}

ThrottleCheckResult YourThrottle::WillStartRequest() {
  if (CheckURL(navigation_handle()->GetURL()).action() == BLOCK_REQUEST) {
    return BlockRequestWithPage(navigation_handle()->GetURL());
  }
  return PROCEED;
}

// static
ThrottleCheckResult YourThrottle::BlockRequestWithPage(const GURL& url) {
  g_block_count.fetch_add(1, std::memory_order_relaxed);
  std::string html = BuildBlockPageHTML(
      g_block_count.load(std::memory_order_relaxed));
  return ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT,
                             std::make_optional(std::move(html)));
}
```

**Key advantages:**
- **History is correct.** Chromium's navigation system never adds the blocked URL to the session history tab. Pressing Back goes to the page _before_ the blocked URL.
- **No flash.** The block page HTML replaces the content area immediately without an intermediate error page or blank state.
- **No PostTask needed.** The decision is synchronous and atomic.
- **Works on all platforms** (Android back button, desktop back key, mouse gesture).

**⚠️ CRITICAL: NO JAVASCRIPT EXECUTES ON ERROR PAGES.** Chromium strips all scripts from custom error page content served via `BLOCK_REQUEST` — this includes:
  - `onclick="..."` inline event handlers (silently ignored)
  - `<script>` tags (never executed)
  - `href="javascript:..."` (never navigated)

This is a host-side security restriction of the error page mechanism, not a bug you can work around.

**What this means for your block page:**
- ❌ A "Back to previous page" button with JS will do nothing
- ❌ A `window.history.back()` call will never execute
- ❌ Any dynamic JS behavior (fetch, DOM manipulation, analytics) is impossible
- ✅ CSS works perfectly — inline styles, `background-image`, dark mode, animations
- ✅ The Android/device back button works — it uses Chromium's native `NavigationController::GoBack()`, which bypasses the error page's JS sandbox

**Design accordingly:** Use a static hint like "Press the back button to return" instead of a button. If you need interactive WebUI features (message handlers, `loadTimeData`, proper buttons), use the Fallback (PostTask → WebUI) approach instead, but be aware of the history corruption bug documented in that section.

**⚠️ CRITICAL: Stale `.o` files owned by root prevent incremental rebuilds**

Docker containers run as root. All `.o` and `.a` files in `out/Default/` are owned by `root:root`. On the host, you modify source files as `hari`, giving them a new timestamp. But inside the container, `ninja` compares the source timestamp against the `.o` timestamp. If the `.o` (root-owned) is **newer** than the source, `ninja` skips with `ninja: no work to do`.

**Symptom:** You change a `.cc` file, run `docker compose run`, see `ninja: no work to do` — but the old object code is linked into `libchrome.so`. The user still sees the old behavior.

**Root cause:** The `.o` file was created by a previous Docker run (as root) and has a later timestamp than your host-side edit. Since `ninja` runs as root in the container and the `.o` is newer, it thinks nothing changed.

**Fix — delete ONLY the SPECIFIC `.o` files for the files you changed:**
```bash
docker run --rm -v ./checkout:/checkout alpine:latest sh -c "
  rm -f /checkout/src/out/Default/obj/chrome/browser/navigation_policy/*.o
  rm -f /checkout/src/out/Default/obj/chrome/browser/ui/ui/distraction_blocked_ui.o
"
```

**⚠️⚠️⚠️ NEVER delete ALL `.o` files.** Doing so triggers a FULL rebuild of all ~50,000 targets, taking **6-10+ hours** on a mid-range machine. This is the single most destructive mistake in Chromium development — always delete only the specific `.o` files corresponding to the modified source files. If you need to find the right `.o` path, run `write_file` to create the modified file, then `docker compose run builder ninja -t commands modified_file.cc` to see exactly which `.o` it compiles to, or just delete the whole subdirectory tree under `obj/chrome/browser/navigation_policy/`.

**To verify the rebuild is real:** Run `strings` on the output `libchrome.so` and grep for old strings BEFORE and AFTER the fix. Zero matches = clean.

**Prevention:** After `cp` or `touch` on host source files, always run `docker compose run --rm ... builder ninja ...` and check the first output line. If it says `ninja: no work to do` when you know you changed something, delete the specific stale `.o` files and retry.

**⚠️ HTML duplicated in TWO source files**

The block page HTML string is embedded in `shorts_reels_blocker.cc :: BuildBlockPageHTML()` AND `distraction_blocked_ui.cc :: BuildPageHTML()`. They serve different flows (throttle error page vs. SPA WebUI redirect). When updating visuals (icon, wording, counter text, removing/adding buttons), ALWAYS update BOTH files or grep for the changed string across both.

**⚠️ Error page JS doesn't execute — inline event handlers, `<script>` tags, and `javascript:` URIs are all stripped by Chromium's error page security sandbox.**
- `#include "net/base/net_errors.h"` for `net::ERR_BLOCKED_BY_CLIENT`
- `#include "base/memory/ref_counted_memory.h"` if using `RefCountedString` for WebUI serving
- Build HTML with `base::StrCat` (NOT `base::StringPrintf` with `%s` in raw strings — triggers compile-time format spec checking)

**⚠️ Raw string `R"(...)` gotcha with data URIs:**

When the HTML contains data URIs with SVG, the `)"` sequence inside the CSS (`%3E"`) prematurely terminates `R"(...)`. Always use a custom delimiter:

```cpp
// ❌ BROKEN — data URI's )" ends the raw string early
constexpr auto kCss = R"CSS(
  .icon { background-image: url("data:image/svg+xml,...%3E"); }
)CSS";  // ^^ )" here ends R"CSS(...) early!

// ✅ WORKS — custom delimiter with no )" in the content
constexpr auto kCss = R"CSS(
  .icon { background-image: url("data:image/svg+xml,...%3E"); }
)CSS";  // )CSS" only matches the delimiter
```

Symptom: inexplicable compile error `expected unqualified-id` at the raw string closing line. Always check data URIs for `)"`.

### Fallback: redirect throttle → WebUI via PostTask (use only if WebUI features like loadTimeData are needed)

⚠️ **KNOWN BUG:** This approach corrupts the navigation history stack — pressing the device's back button reloads the block page instead of going to the previous page. Only use when `BLOCK_REQUEST` with inline HTML is insufficient (e.g., you need full WebUI features like `loadTimeData`, `chrome.send()`, or message handlers).

Never call `LoadURLWithParams` synchronously from within a throttle callback. Always defer:

```cpp
// REQUIRED includes:
#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"

ThrottleCheckResult YourThrottle::WillStartRequest() {
  if (CheckURL(navigation_handle()->GetURL()).action() == BLOCK_REQUEST) {
    content::WebContents* wc = navigation_handle()->GetWebContents();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&YourThrottle::NavigateToBlockPage, wc));
    return CANCEL_AND_IGNORE;
  }
  return PROCEED;
}
```

Key details:
- **`CANCEL_AND_IGNORE`** cancels the current navigation without showing an error page
- The `PostTask` runs AFTER the throttle returns, when the navigation system is stable
- **`content::GetUIThreadTaskRunner` is in `content/public/browser/browser_thread.h`** — NOT in `browser_task_traits.h`. Using the wrong header gives: `error: no member named 'GetUIThreadTaskRunner' in namespace 'content'`
- Include: `#include "chrome/common/webui_url_constants.h"`; navigate with `GURL("chrome://distraction-blocked")`

TabHelper follows the same pattern (no deferral needed since it's already async from `DidFinishNavigation`):
```cpp
void YourTabHelper::MaybeBlockURL(const GURL& url) {
  if (CheckURL(url).action() != BLOCK_REQUEST)
    return;
  YourThrottle::NavigateToBlockPage(web_contents());
}

**⚠️ SPA pushState: use `should_replace_current_entry = true`** — when the TabHelper intercepts a pushState navigation, the blocked URL is already committed to the history stack. Without replacing it, pressing Back from the block page re-navigates to the blocked URL, triggering the TabHelper again (infinite loop). Set `should_replace_current_entry = true` on the `LoadURLParams` to replace the blocked URL entry.
```

### Reference implementation

See `references/creating-custom-webui.md` for a complete step-by-step with code examples. Reference files at `chrome/browser/ui/webui/distraction_blocked/`.

## Pitfalls & Troubleshooting

### Build time expectations by CPU

Build times vary enormously by CPU. Real data from a session:

| CPU | Cores | RAM | Targets | Time |
|-----|-------|-----|---------|------|
| i5-6500T (35W TDP) | 4 @ 2.5GHz | 15GB | 50,245 (Profile B) | **~5h+** for full rebuild |
| Ryzen 7 3700X | 8 @ 3.6GHz | 30GB | 50,245 (Profile B) | **~1-2h** estimated |
| Modern Xeon/Threadripper | 16+ | 64GB+ | 50,245 | **~30-45 min** estimated |

On low-core CPUs with `-j4`, expect ~1,000-2,000 targets per hour initially (C++ compile-heavy phase), then faster as V8/Blink object files accumulate. The `.o` file count (`find out/Default -name '*.o' -newer args.gn | wc -l`) is a more reliable progress indicator than ninja's target count.

Check thermal health during the build:
```bash
cat /sys/class/thermal/thermal_zone0/temp
```
Each reading is millidegrees Celsius (e.g. 65000 = 65°C). The i5-6500T ran at a steady 65°C for hours under `-j4` — well within safe limits (critical at ~100°C).

### RAM pressure at link stage
- The final link of `libchrome.so` is the most memory-intensive step
- If the build OOMs, reduce `symbol_level` further or use a swap file
- The `blink/v8_symbol_level=0` flags are the single biggest RAM saver (saves gigabytes at link time)

#### ⚠️ Real-world OOM scenario (16GB RAM, Profile B, -j4)

Tested on a machine with **15.7GB RAM + 54GB swap**. Profile B (`is_official_build=true` = LTO enabled) with `-j4`:

- **During compilation phase**: 4 clang++ processes, each using 2–4GB → easily hits 12GB+ RAM. With swap, it survives.
- **At final link stage (LTO)**: The linker (`lld`) needs **4–6GB** just for the final `libchrome.so` link. Combined with remaining compile jobs still running, total RAM consumption spikes above 16GB.
- **Result**: Container exits silently (exit code lost with `docker compose run --rm`). APK is NOT updated. `.ninja_log` shows activity up to `~45K .o files` but no link-stage entries. No OOM message in `dmesg` or `journalctl` — the Docker container just dies when the kernel's OOM killer targets the container's cgroup.

**Fix on 16GB-class machines:**

```bash
# Option 1: Fewer parallel jobs
ninja -j3 -C out/Default chrome_public_apk    # -j3 instead of -j4

# Option 2: More swap (pre-allocate disk swap)
sudo fallocate -l 32G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# Option 3: Skip LTO if RAM is tight (Profile A instead of B)
# Set use_thin_lto=true and is_official_build=false to avoid full LTO link

### Git safe.directory errors

Bind-mounted directories have different ownership inside Docker. The Dockerfile pre-configures all directories as safe:

```dockerfile
RUN git config --global --add safe.directory '*'
```

The wildcard `*` is essential — third-party submodules under `src/third_party/*` also trigger dubious ownership errors, and listing them all is impractical. If you still get errors (e.g. when running `gclient runhooks` inside the container):

```bash
git config --global --add safe.directory /checkout
```

### Makefile bash quoting (⚠️ critical)

Multi-line bash commands inside Makefiles are **dangerous**. This pattern will BREAK:

```makefile
# ❌ BROKEN — Make's trailing `\` concatenates lines before bash sees them
make target:
	docker compose run --rm builder bash -c '\
		echo "first" && \
		echo "second"
	'
```

The `\` at the end of Make recipe lines is Make's line continuation — it strips the newline, and bash sees everything as one mangled line with no closing quote.

**Fix:** Put everything on a single line with `;` separators:

```makefile
# ✅ WORKS — single line
make target:
	docker compose run --rm builder bash -c 'echo "first"; echo "second"; third_command'
```

### File ownership after fetch (Docker runs as root)

`docker compose run` runs as root by default. All files created by `fetch`, `gclient sync`, and `gclient runhooks` inside the container will be owned by `root:root` on the host. This prevents host-side commands (like `mkdir` + writing `args.gn`) from working.

**Fix:** After fetch/deps steps, chown the checkout:

```bash
docker compose run --rm -u root builder chown -R $(id -u):$(id -g) /checkout/src
```

### Rust sysroot missing for Android std lib

Building for Android requires the Rust compiler to build `core`, `alloc`, etc. for `aarch64-linux-android`. The output path `out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib/` must exist before rustc runs — rustc does not create intermediate directories.

**Symptom:** `FAILED: local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib/libcore_core.rlib` followed by `FileNotFoundError` in `rustc_wrapper.py` line 407 (can't open depfile).

**Fix:** Create the directory before building:
```bash
mkdir -p out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib
```
Add this to the Makefile's `configure` or `build` target. One-time fix — persists once created.

### `autoninja` Python 3.10 StrEnum crash

`autoninja` (the depot_tools wrapper) imports `from enum import StrEnum` — `StrEnum` was added in Python 3.11. The container runs Ubuntu 22.04 with Python 3.10, so autoninja crashes immediately.

**Symptom:** `ImportError: cannot import name 'StrEnum' from 'enum' (python3.10/enum.py)`

**Fix:** Use `ninja` directly: `ninja -j4 -C out/Default chrome_public_apk`. The `-j` value should match `nproc` on your machine.

### `android_static_analysis` must be `"off"` with direct `ninja`

When using `ninja` directly (not `autoninja`), the `AUTONINJA_BUILD_ID` environment variable is never set. The errorprone Java static analysis tool requires this variable for build-server integration.

**Symptom at build finish (96%+ targets done):**
```
Exception: AUTONINJA_BUILD_ID is not set. android_static_analysis = build_server
requires autoninja integration.
FAILED: obj/base/activity_state_java__errorprone.stamp
```

**Fix:** Set `android_static_analysis = "off"` in `args.gn`. This is not merely a performance optimization — without it, the build **will fail** at the very end during Java analysis.

```gn
android_static_analysis = "off"
```

Note: valid values are `"on"` or `"off"` only — `"none"` will produce an assertion error at GN regeneration time.

### depot_tools bootstrap for Docker

depot_tools CIPD packages (including its own Python) are never downloaded inside Docker with `DEPOT_TOOLS_UPDATE=0`. The script `python-bin/python3` reads `python3_bin_reldir.txt` to find Python — if missing, all depot_tools wrappers (gn, ninja) fail.

### `ThrottleCheckResult` comparison operator

`NavigationThrottle::ThrottleCheckResult` does NOT have an `operator==` for comparing with `ThrottleAction` enum values:

```cpp
// ❌ BROKEN
ThrottleCheckResult result = CheckURL(url);
if (result == PROCEED) { ... }

// ✅ CORRECT
if (result.action() == PROCEED) { ... }
```

Symptom: `error: invalid operands to binary expression ('ThrottleCheckResult' and 'content::NavigationThrottle::ThrottleAction')`
(GURL's operator== is tried as a candidate and also fails, making the error message confusing.)

### Misleading exit code from Makefile

The Makefile always prints `✓ Build complete! APK at ...` regardless of whether `ninja` succeeded or failed. This is because `make` uses `tee -a` (pipeline) which masks the real exit code. Ninja's exit code is lost.

**Don't trust the Makefile output** — always check the build log for `FAILED:` or `error:` at the end:

```bash
tail -20 ~/chromium-android/build-logs/build.log | grep -E 'FAILED|error:|ninja: build stopped'
```

The actual signal is:
- `ninja: build stopped: subcommand failed` → build failed
- `✓ Build complete! APK at ...` → build **actually** succeeded (this line is correct only when ninja exits 0, but the tee wrapper lies in the failure case)

### Diagnosing a container-exited-before-completion build

When the container auto-removed itself (`docker compose run --rm`) and you're not sure if the build succeeded:

```bash
# 1. Check if the APK was regenerated (timestamp tells the story)
stat ~/chromium-android/checkout/src/out/Default/apks/ChromePublic.apk | grep Modify
# If this timestamp is BEFORE the container started → build didn't finish

# 2. Check .ninja_log last modification time
stat ~/chromium-android/checkout/src/out/Default/.ninja_log | grep Modify
# If newer than APK → build made progress but stopped short
# This is the #1 quick check — .ninja_log is written to on every completed edge,
# and is on the bind mount so it persists after the container exits

# 3. Check last entries in .ninja_log — do they end with .o files or link steps?
tail -5 ~/chromium-android/checkout/src/out/Default/.ninja_log
# If last entries are all `.o` files → still in compilation phase, never reached linking
# If last entries contain `.a` / `.so.TOC` / `apk` entries → build got further

# 4. Count .o files to see total progress
find ~/chromium-android/checkout/src/out/Default -name '*.o' | wc -l
# The 50K+ range means it got well into the build

# 5. Check for OOM (no output = likely not OOM-killed on host)
dmesg -T 2>/dev/null | grep -i "oom\|killed"
journalctl -k 2>/dev/null | grep -i "oom\|killed"
```

**Common scenario**: Build ran for 9+ hours, compiled ~45K `.o` files, container exited during link stage. System memory was 12GB/15.7GB used, swap pressure 3.5GB/54GB. The Docker container's cgroup got OOM-killed silently before the final link completed — no host-level OOM message because only the container's cgroup was hit, not the entire system.

### Restarting a failed build

Good news: **ninja is incremental**. If the build died mid-way, just re-run it:

```bash
# Inside the container or via Makefile:
make build   # ninja picks up from where it left off
```

Ninja only rebuilds what's actually needed. The `.ninja_log` records every completed build edge, so failed targets are re-run. The second run goes much faster — typically only ~25% of total targets remain (the final linking + APK packaging).

**If the first container ran with `--rm` and disappeared**, the bind mount still has all the build artifacts. Starting a new `docker compose run` / `make build` will find them and continue.

**If OOM was the issue, reduce -j on the retry:**
```bash
# In Makefile, override the ninja flag:
docker compose run --rm builder ninja -j3 -C out/Default chrome_public_apk
# Or update the Makefile's build target to use -j3
```

**⚠️ Real-world validation (16GB RAM, Profile B):**
- `-j4` reached ~45K `.o` files then silently OOM'd at the LTO link stage (cgroup OOM-kill, no host-level dmesg message)
- `-j3` completed the same remaining ~14K targets successfully
- Use `-j3` as the default on any machine with <20GB RAM
**⚠️ Avoid duplicate container instances:**

When restarting via `terminal(background=true, notify_on_complete=true)`, `docker compose run` can spawn **two containers** for the same build if the first isn't fully killed. Two ninja instances writing to the same `out/Default/` directory will corrupt the build.

**Fix:** Always kill any existing chromium containers FIRST, and verify with `docker ps` before starting:

```bash
# Before starting a new build — kill ALL chromium containers
docker kill $(docker ps --filter name=chromium --format '{{.ID}}') 2>/dev/null
docker rm $(docker ps -a --filter name=chromium --format '{{.ID}}') 2>/dev/null
```

**⚠️ `docker compose run --name` fails silently on stale containers.**

If a previous build exited mid-way (OOM, timeout, interruption), the `--name chromium-builder` container may still exist in `docker ps -a` even though `docker ps` doesn't show it. The next `docker compose run --name chromium-builder --rm` **will fail** with:

```
Error response from daemon: Conflict. The container name "/chromium-builder" is
already in use by container "...". You have to remove (or rename) that container
to be able to reuse that name.
```

This error is **not shown in the build log** because `terminal(background=true, ...)` truncates output at the last 1978 chars, and the `docker compose run` actually never starts — the earlier cleanup commands (echo, kill, rm) succeed and produce the exit code seen by the process monitor.

**Fix — always run cleanup BEFORE `docker compose run`:**

```bash
# Single-line cleanup that handles all cases:
docker rm -f chromium-builder 2>/dev/null

# Or broader sweep for any chromium-named container:
docker kill chromium-builder 2>/dev/null; docker rm chromium-builder 2>/dev/null
```

**Verification:** After cleanup, `docker ps --filter name=chromium --format '{{.Names}}'` should return nothing. Then start the build.

### Logging for long-running tasks
- Source: ~15-30GB
- Build output (release, no V8/Blink symbols): ~20-40GB
- Total: ~35-70GB
- Check with `df -h /` before starting

### Incremental builds
After the first build, small changes rebuild in minutes. Use the container's cached build artifacts.
### Reconfiguring

To build for x86 (emulator) instead of arm64:

```bash
make reconfigure ARGS='target_os="android" target_cpu="x86" symbol_level=1 blink_symbol_level=0 v8_symbol_level=0 is_debug=false treat_warnings_as_errors=false'
```

## Settings stripping: the addPreferenceIfAbsent trap (v20 → v21 production bug)

When you strip preference keys from `main_preferences.xml`, the Java code references them in two places:

1. **`createPreferences()`** — uses `findPreference(key)` which returns null → just null-guard the result
2. **`updatePreferences()`** — uses `addPreferenceIfAbsent(key)` which calls `mAllPreferences.get(key)` followed by `assumeNonNull()`

`cachePreferences()` only populates `mAllPreferences` from XML keys. Keys stripped from XML are **never cached**. When `updatePreferences()` calls `addPreferenceIfAbsent(stripped_key)` at runtime, it crashes with NPE — `assumeNonNull(mAllPreferences.get("settings_promo_card"))` explodes.

**The build will NOT catch this** — it compiles fine, crashes only when the user opens Settings.

**Fix (v21):** Remove the entire `addPreferenceIfAbsent` call AND its conditional block from `updatePreferences()`. The key doesn't exist in the pref tree — there's nothing to add, remove, or update. Just delete the code block.

Keys affected in this build: `PREF_SETTINGS_PROMO_CARD`, `PREF_SIGN_IN`, `PREF_GOOGLE_SERVICES`.

## Platform awareness when patching build flags

Before setting or modifying a build flag (e.g. `enable_dice_support`), **verify the flag is relevant to the target platform**. Many flags are desktop-only:

- DICE (cross-device sign-in) is desktop-only — Android uses `AccountManager`. The GN default is already `false` for Android.
- Chrome sign-in dialogs (FRE, History Sync, promos) are Android-specific, controlled by Java-side code, not build flags.

**How to check:** Search for the flag in source:
```bash
grep -rl "ENABLE_DICE_SUPPORT" src/ --include="*.cc" --include="*.h" | head -10
# If all results are in desktop-specific dirs, flag is irrelevant
```

Focus patches on the code paths that actually run on Android — `chrome/android/`, `chrome/browser/ui/android/`, `components/signin/internal/identity_manager/`.

## Cleanup

```bash
make clean    # remove containers, keep source
make nuke     # remove everything + Docker image
# OR simply:
rm -rf ~/chromium-android
```

## References

- Official docs: [Android Build Instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/android_build_instructions.md)
- GN build config: [GN Build Configuration](https://www.chromium.org/developers/gn-build-configuration)
- `references/patch-verification.md` — Inspect real source before writing patches
- `references/navigation-throttle-pattern.md` — Modern NavigationThrottleRegistry API
- `references/error-transcripts.md` — Full error transcripts from a real build session (BUILD.gn missing, GURL include, android_static_analysis)
- `references/creating-custom-webui.md` — Step-by-step for adding WebUI pages with inline strings (chrome://distraction-blocked reference)
- `references/distraction-blocker-implementation.md` — Full distraction blocker implementation (throttle + content injection, all platforms, CSS/JS payloads)
- `templates/` — Dockerfile, docker-compose.yml, Makefile starter templates
- `scripts/apply-patches.py` — Reusable patch-application script (copy + registration + BUILD.gn patching)
- `src/components/neterror/resources/neterror.html` — Lit-based error page template; use as base for custom interstitial pages
