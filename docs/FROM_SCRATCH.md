# The Chromium-for-Android Custom Build Guide

### Building Your Own Browser, From Zero to APK

**Your story:** You are Hari. You wanted a browser that blocks short-form video garbage — YouTube Shorts, Instagram Reels, TikTok — everywhere, at the network level, so it works in every app that opens Chrome custom tabs. You also wanted a clean New Tab Page with just a logo and search bar, zero Google sign-in surfaces, notifications denied by default, and no Password Manager or Google Services in Settings. You knew nothing about Chromium's 30GB source tree when you started.

Six days later, after 25 APK builds, 14,336 ninja steps, 16 patches, and countless build crashes, you had it.

This guide is the full story of that journey — every command, every error, every fix. You can follow it from scratch and build the same browser.

---

## Chapter 1: What Are We Actually Building?

### The pitch

When you install Chrome from the Play Store, you get a browser designed by Google for Google. It has:

- A New Tab Page stuffed with news feed, most-visited tiles, AI mode, and sign-in promos
- YouTube Shorts, Instagram Reels, TikTok — all fully accessible
- Notification prompts on every site that asks
- Google Password Manager, Google Services, sign-in buttons everywhere
- Custom Geist fonts that add 5+ MB to the APK
- A profile icon in the toolbar that invites you to sign in

**We wanted none of this.** We wanted:

| What we want | How we do it |
|---|---|
| Block YouTube Shorts (but keep normal YouTube) | Navigation throttle blocks `/shorts` URL prefix |
| Block Instagram Reels | Navigation throttle blocks `/reels/` URL prefix |
| Block TikTok entirely | Whole-domain block |
| Block Facebook/Reddit/LinkedIn Reels | Per-platform rule files |
| Catch SPA navigations (pushState) | TabHelper watches DidFinishNavigation |
| NTP = logo + search bar only | Silent ViewStub Hiding (keep XML IDs, set gone+0dp, skip Java init) |
| No sign-in anywhere | 10 patches across 8 surface areas |
| Notifications default-deny | ContentSettingsRegistry default = BLOCK |
| No Password Manager in Settings | Remove XML preferences + skip Java sections |
| No Google Services in Settings | Remove from XML + hide sign-in toggle |
| No profile icon in toolbar | IdentityDiscController always returns canShow=false |
| Block page showing count | chrome://distraction-blocked WebUI with live counter |
| Hide Reels even on SPA sites | CSS/JS injection at DOMContentLoaded via isolated world |

**The constraint:** We build against a *stable* Chromium release tag (not `main`), so our patches don't break every 4 weeks. This guide targets **Chromium 149.0.7827.84**.

### How long it takes

| Step | Time | Notes |
|---|---|---|
| Docker image build | 2 minutes | One-time |
| Source fetch | 30 minutes | ~15 GB download, ~30 GB on disk |
| First full build | 6-10 hours | 14,336 ninja steps |
| Incremental build (C++ change) | 10-30 minutes | 50-500 steps |
| Incremental build (Java change) | 1-5 minutes | ~50 steps |
| Incremental build (XML change) | 30-60 seconds | Resource repackaging only |

### What you end up with

A 350 MB `ChromePublic.apk` you can sideload on any Android phone. It looks and feels like Chrome — same speed, same rendering engine, same DevTools — but:

- Click a YouTube Shorts link → red block page with counter ("blocked 47 times this session")
- Open a new tab → Google logo + search bar, nothing else
- Open Settings → No "Sign in" button, no "Google Services", no "Password Manager"
- Visit a site → no notification permission prompt (it's already blocked by default)
- Open Instagram Reels → nothing, blocked at the network level

---

## Chapter 2: The Chromium Codebase — Your New Home

### What you're dealing with

```
git clone https://chromium.googlesource.com/chromium/src.git
# 1.2 million commits, 30 GB, 10K+ C++ files, 50K+ Java files, 300K+ XML resources
```

You don't need to understand all of it. You need to understand **three layers**:

```
 ┌─────────────────────────────────────────────────┐
 │  Layer 3: chrome/android/                      │
 │  Java + XML + resources for Android UI         │
 │  (NTP, Settings, toolbar, etc.)                │
 ├─────────────────────────────────────────────────┤
 │  Layer 2: chrome/browser/                      │
 │  C++ browser UI layer (throttles, navigation)  │
 ├─────────────────────────────────────────────────┤
 │  Layer 1: content/                             │
 │  Web engine (rendering, navigation, processes) │
 └─────────────────────────────────────────────────┘
```

**Layer 3 (`chrome/android/`)** is where most of our work lives. This is Android-specific Java code organized by feature:

```
chrome/android/java/src/org/chromium/chrome/browser/
  ├── ntp/           ← New Tab Page (feed, tiles, search bar, sign-in promo)
  ├── settings/      ← MainSettings, password manager, appearance
  ├── sync/          ← Google Services settings, sync opt-in
  ├── identity_disc/ ← Profile icon button in toolbar
  ├── toolbar/       ← Sign-in button, toolbar shortcut
  ├── flags/         ← chrome://flags
  └── ...
```

**Layer 2 (`chrome/browser/`)** handles C++ navigation logic. This is where our navigation throttle hooks in:

```
chrome/browser/
  ├── navigation_policy/   ← OUR CUSTOM MODULE (shorts/reels blocker)
  ├── content_injection/   ← OUR CUSTOM MODULE (CSS/JS injection)
  ├── ui/webui/distraction_blocked/ ← OUR CUSTOM WEBUI PAGE
  └── chrome_content_browser_client_navigation_throttles.cc ← WE PATCH THIS
```

**The golden rule of Chromium navigation:** Every navigation goes through a single throttle registration point. Find it at `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`. Every custom throttle you add must be registered here.

### Finding what you need

```
# Need to find where a UI string is used?
grep -rn "notification_permission" chrome/android/java/res/values/strings.xml

# Need to find which Java file inflates a layout?
grep -rn "R.layout.new_tab_page_layout" chrome/android/java/src/

# Need to find a C++ class?
grep -rn "class NavigationThrottle" chrome/browser/
```

**The `R.id` trap:** Every XML element with `android:id` generates a compile-time constant in `R.java`. If you delete an XML element, the `R.id.xxx` constant disappears. Any Java code referencing it — even dead code that never runs — will fail to compile. This is the single most important thing to understand about Android resource modification.

---

## Chapter 3: Setting Up the Build Environment

### The Docker approach

Chromium needs exact tool versions: JDK 17, Python 3.8+, Node.js 20 LTS, Depot Tools, and 50+ system packages. Docker wraps everything in a clean container. One `rm -rf ~/chromium-android` deletes everything.

### Directory layout

```
~/chromium-android/
├── Dockerfile              # Build container definition
├── docker-compose.yml      # Volume mounts, SHM size, etc.
├── Makefile                # One-command workflow (make fetch → make build)
├── build-logs/             # All build output logs saved here
├── depot_tools/            # Google's build tools (cloned once)
├── checkout/               # Chromium source (30GB+)
│   ├── .gclient            # Depot tools config
│   └── src/                # ← THE ACTUAL CHROMIUM SOURCE
│       └── out/Default/    # ← BUILD OUTPUT + APK
├── serve/                  # HTTP server for APK download
└── patches/                # ← OUR GIT REPO (this is what you push to GitHub)
    ├── source-files/       # Our custom C++/WebUI modules (never exist upstream)
    ├── patches/            # .patch files for modified upstream files
    ├── build-config/       # Infrastructure files (Dockerfile, Makefile, etc.)
    ├── docs/               # This guide
    ├── apply-patches.py    # Script: applies all patches to checkout
    └── generate-patches.py # Script: creates .patch files from modified checkout
```

### Step 1: Create the workspace

```bash
mkdir -p ~/chromium-android && cd ~/chromium-android

# Clone our patches repo (the output of this entire guide)
git clone https://github.com/hxri-nxrxyxn/chromium.git patches

# Copy infrastructure files to workspace root
cp patches/build-config/Dockerfile .
cp patches/build-config/docker-compose.yml .
cp patches/build-config/Makefile .
cp patches/build-config/.gclient checkout/
```

### The Dockerfile (complete)

```dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/depot_tools:${PATH}"

# Install base tooling required by Chromium's build system
RUN apt-get update && \
    apt-get install -y \
        curl git lsb-release python3 \
        python3-distutils file sudo vim \
        apt-transport-https ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Mark all directories as safe Git directories (needed for bind mounts)
RUN git config --global --add safe.directory '*'

# Working directory matches Chromium source root
WORKDIR /checkout/src
```

**Why so minimal?** The Docker image only provides the OS baseline. Chromium's own `install-build-deps.sh` installs the 50+ packages for C++ compilation. Depot Tools (fetched separately on the host) provides `gn`, `ninja`, and `gclient`.

**Why Ubuntu 22.04 and not 24.04?** Because Chromium's build scripts have been tested against 22.04. Newer Ubuntu versions may have package version mismatches.

### The docker-compose.yml (complete)

```yaml
version: "3.9"

services:
  builder:
    build:
      context: .
      dockerfile: Dockerfile
    image: chromium-android-builder
    container_name: chromium-android
    volumes:
      - ./depot_tools:/depot_tools
      - ./checkout:/checkout
    working_dir: /checkout/src
    stdin_open: true
    tty: true
    shm_size: 2g    # Chromium's linker needs lots of shared memory
    environment:
      - DEBIAN_FRONTEND=noninteractive
      - DEPOT_TOOLS_UPDATE=0  # skip depot_tools self-update (faster)
    cap_add:
      - SYS_PTRACE   # debugging support
```

**Key details:**
- `shm_size: 2g` — Without this, the linker crashes with "cannot allocate memory" errors. Chromium's linker uses POSIX shared memory for huge object files.
- `DEPOT_TOOLS_UPDATE=0` — Without this, every `docker compose run` call spends 10 seconds updating depot_tools.
- `SYS_PTRACE` — Needed for Chromium's crash handling infrastructure.

### The Makefile (complete)

```makefile
# ── Configuration ──────────────────────────────────────────────
BUILD_DIR ?= out/Default
CHROMIUM_VERSION ?= 149.0.7827.84
CHROMIUM_TAG ?= refs/tags/$(CHROMIUM_VERSION)
LOG_DIR ?= build-logs

# ── Setup ──────────────────────────────────────────────────────
setup: depot_tools image

depot_tools:
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

image:
	docker compose build

# ── Fetch source (~30 min) ────────────────────────────────────
fetch: | $(LOG_DIR)
	mkdir -p checkout
	docker compose run --rm builder bash -c "\
		cd /checkout && \
		fetch --nohooks --no-history android 2>&1 && \
		gclient sync --revision src@$(CHROMIUM_TAG) 2>&1 \
	" | tee -a $(LOG_DIR)/fetch.log

# ── Apply patches ─────────────────────────────────────────────
patches:
	python3 patches/apply-patches.py "$(PWD)/checkout/src" "$(PWD)/patches"

# ── Install deps + run hooks ──────────────────────────────────
deps:
	docker compose run --rm builder bash -c "\
		./build/install-build-deps.sh 2>&1 \
	" | tee -a $(LOG_DIR)/deps.log
	docker compose run --rm builder \
		gclient runhooks 2>&1 | tee -a $(LOG_DIR)/deps.log

# ── Configure build ───────────────────────────────────────────
configure:
	mkdir -p checkout/src/out/Default
	printf '%s\n' \
		'target_os = "android"' \
		'target_cpu = "arm64"' \
		'symbol_level = 1' \
		'blink_symbol_level = 0' \
		'v8_symbol_level = 0' \
		'is_debug = false' \
		'treat_warnings_as_errors = false' \
		'android_static_analysis = "off"' \
		> checkout/src/out/Default/args.gn
	docker compose run --rm builder bash -c 'gn gen /checkout/src/out/Default'

# ── Build APK (6-10 hours!) ──────────────────────────────────
build: | $(LOG_DIR)
	docker compose run --rm -e GCLIENT_SUPPRESS_GIT_VERSION_WARNING=1 builder \
		ninja -j3 -C $(BUILD_DIR) chrome_public_apk 2>&1 \
		| tee -a $(LOG_DIR)/build.log

# ── Utilities ─────────────────────────────────────────────────
shell:
	docker compose run --rm builder bash

install:
	docker compose run --rm builder \
		$(BUILD_DIR)/bin/chrome_public_apk install

clean:
	docker compose down -v 2>/dev/null || true

nuke: clean
	rm -rf checkout depot_tools
	docker rmi chromium-android-builder 2>/dev/null || true
```

### Step 2: Build the Docker image

```bash
cd ~/chromium-android
make setup
# This runs: git clone depot_tools + docker compose build
# Takes ~2 minutes.
```

### Step 3: Fetch the source

```bash
make fetch
# ===== Fetching Chromium 149.0.7827.84 (stable) for Android =====
#     Log: /home/hari/chromium-android/build-logs/fetch.log
# Docker runs: fetch --nohooks --no-history android
# Then: gclient sync --revision src@refs/tags/149.0.7827.84
```

**What happens inside:**
1. `fetch android` creates the `.gclient` file and checks out the `main` branch
2. `gclient sync --revision src@refs/tags/149.0.7827.84` pins to our stable tag
3. This downloads about 15 GB of source code
4. The checkout directory ends up ~30 GB on disk

**Expected errors during fetch:**

| Error | Why | Fix |
|---|---|---|
| `rate limit exceeded` | Google Source API rate limiting | Wait 60 seconds, retry with `gclient sync --jobs 1` |
| `refs/tags/149.0.7827.84 not found` | Tag doesn't exist yet | Check https://chromiumdash.appspot.com/releases for the latest stable |
| `error: RPC failed; curl 56 GnuTLS recv error` | Network timeout | Retry — gclient resumes from where it left off |

### Step 4: Install build dependencies

```bash
make deps
# Runs install-build-deps.sh (installs 50+ system packages)
# Then runs gclient runhooks (generates build files)
# Takes ~10 minutes
```

### Step 5: Configure the build

```bash
make configure
# ===== Generating Ninja build files =====
# Args: target_os=android, target_cpu=arm64, symbol_level=1, is_debug=false
# gn gen /checkout/src/out/Default
```

**args.gn explained:**

| Flag | Value | Effect |
|---|---|---|
| `target_os` | `"android"` | Cross-compile for Android |
| `target_cpu` | `"arm64"` | ARM 64-bit (covers ~95% of modern phones) |
| `symbol_level` | `1` | Minimal debug symbols (saves 20+ GB vs level 2) |
| `blink_symbol_level` | `0` | No Blink rendering engine symbols |
| `v8_symbol_level` | `0` | No V8 JavaScript engine symbols |
| `is_debug` | `false` | Release build (optimized, no asserts) |
| `treat_warnings_as_errors` | `false` | Prevents compiler warnings from crashing the build |
| `android_static_analysis` | `"off"` | Skip Android Lint (saves 30+ minutes per build) |

---

## Chapter 4: First Build — Getting the APK Out

### Building for the first time

```bash
cd ~/chromium-android
make build
# docker compose run --rm builder ninja -j3 -C out/Default chrome_public_apk
```

**This takes 6-10 hours.** Go to sleep. Watch a movie. The terminal will show:

```
[1/14336] CXX obj/chrome/browser/somefile.o
[500/14336] ACTION //chrome/android:chrome_apk_pak_assets
[2500/14336] SOLINK ./libchrome.so
...
[14336/14336] ACTION //chrome/android:chrome_public_apk__create
```

**Understanding the output:**

| Prefix | Meaning |
|---|---|
| `CXX` | Compiling a C++ file |
| `ACTION` | Running a build script (dex, resource packaging, APK signing) |
| `LINK` | Linking an executable |
| `SOLINK` | Producing `libchrome.so` |
| `STAMP` | Consolidating dependency stamps |
| `AR` | Archiving object files into a `.a` library |

**If you see `chrome_public_apk__create` with no errors**: you have a working APK.

### Finding and serving your APK

```bash
# The APK is at:
ls -lh checkout/src/out/Default/apks/ChromePublic.apk
# -rw-r--r-- 1 root root 350M Jun 2 12:00 ChromePublic.apk

# Serve it for download:
mkdir -p serve
cp checkout/src/out/Default/apks/ChromePublic.apk serve/
cd serve && python3 -m http.server 8080
# Download from: http://YOUR_LAN_IP:8080/ChromePublic.apk
```

### Full rebuild vs incremental build

```
FULL REBUILD (6-10 hours)
  Triggers:
    - gn gen (reconfigures all ninja files)
    - rm -rf out/Default/
    - Changing a widely-included header (.h referenced by 1000+ files)
  What happens: all 14,336+ steps run

INCREMENTAL BUILD (1-30 minutes)
  Triggers:
    - Changing a single .cc file → ~50 steps
    - Changing a Java file → ~50 steps
    - Adding/removing a resource → ~5 steps (resource repackaging only)
  What happens: ninja tracks what changed and only rebuilds what's needed
```

### Docker progress monitoring

```bash
# Check if build is still running
docker ps | grep chromium-android

# Check progress
docker compose run --rm builder bash -c 'tail -1 /checkout/src/out/Default/.ninja_log'

# Kill stale containers (if build was interrupted)
docker kill $(docker ps -q --filter name=chromium-android) 2>/dev/null
docker rm $(docker ps -aq --filter name=chromium-android) 2>/dev/null
```

### Disk management

The checkout + build output easily fills 80+ GB. Critical commands:

```bash
# Clean Docker cache (reclaims 30-50 GB)
docker system prune -a

# Check disk usage
df -h ~/chromium-android

# Nuke checkout from inside Docker (faster than deleting from host)
docker compose run --rm builder bash -c "rm -rf /checkout/* /checkout/.* 2>/dev/null"

# Remove build output only (keeps source for re-configure)
rm -rf checkout/src/out/Default/
```

---

## Chapter 5: Our 16 Patches — Every Single One Explained

### The system: Three types of changes

**Type A: Brand new files (source-files/)** — Files that don't exist in Chromium upstream. We create them from scratch:

```
source-files/chrome/browser/navigation_policy/
  ├── shorts_reels_blocker.h        (header — throttle + tab helper + wrapper)
  ├── shorts_reels_blocker.cc        (implementation — 314 lines)
  ├── BUILD.gn                       (build target definition)
  └── platform_rules/                (per-platform block rules)
       ├── youtube_block_rules.cc    # blocks youtube.com/shorts
       ├── instagram_block_rules.cc  # blocks instagram.com/reels
       ├── facebook_block_rules.cc   # blocks facebook.com/reels
       ├── tiktok_block_rules.cc     # blocks tiktok.com (whole domain)
       ├── reddit_block_rules.cc     # blocks reddit.com/rpan
       ├── x_block_rules.cc          # blocks x.com/i/timeline
       ├── linkedin_block_rules.cc   # blocks linkedin.com/video
       ├── all_block_rules.cc        # aggregator (includes all platforms)
       ├── block_rule_types.h        # shared rule data structures
       └── BUILD.gn

source-files/chrome/browser/content_injection/
  ├── content_injection_manager.h    # WebContentsObserver for CSS/JS injection
  ├── content_injection_manager.cc   # Main injection logic (92 lines)
  ├── content_injection_rules.h      # Aggregated injection rules
  ├── content_injection_rules.cc     # Platform aggregator
  ├── BUILD.gn
  └── platforms/                     # Per-platform injection rules

source-files/chrome/browser/ui/webui/distraction_blocked/
  ├── distraction_blocked_ui.h       # WebUI controller
  └── distraction_blocked_ui.cc      # Block page with live counter (135 lines)
```

**Type B: Modified upstream files (16 .patch files)** — Files that exist in Chromium but need small changes. We record only the diff:

```
patches/
  01-chrome_browser_BUILD.gn.patch                                  # +2 deps
  02-chrome_browser_chrome_content_browser_client_navigation_       # +1 #include +1 call
    throttles.cc.patch
  03-chrome_android_java_res_layout_new_tab_page_layout.xml.patch   # 5 ViewStubs hidden
  04-chrome_android_java_src_org_chromium_chrome_browser_ntp_       # SKIP 4 inits
    NewTabPageCoordinator.java.patch
  05-chrome_android_java_src_org_chromium_chrome_browser_ntp_       # Remove feed surface
    NewTabPage.java.patch
  06-chrome_android_java_src_org_chromium_chrome_browser_ntp_       # Remove site section
    NewTabPageLayout.java.patch
  07-chrome_android_java_src_org_chromium_chrome_browser_settings_  # Remove sign-in, passwords, google services
    MainSettings.java.patch
  08-chrome_android_java_res_xml_main_preferences.xml.patch         # Remove XML entries
  09-components_content_settings_core_browser_                      # NOTIFICATIONS default=BLOCK
    content_settings_registry.cc.patch
  10-components_signin_internal_identity_manager_                   # kSigninAllowed default=false
    primary_account_manager.cc.patch
  11-chrome_android_java_src_org_chromium_chrome_browser_identity_  # Identity disc always hidden
    disc_IdentityDiscController.java.patch
  12-chrome_browser_ui_android_signin_java_src_org_chromium_chrome_ # canShowPromo() = false
    browser_ui_signin_signin_promo_SigninPromoMediator.java.patch
  13-chrome_browser_ui_android_signin_java_src_org_chromium_chrome_ # Skip sign-in + dismiss
    browser_ui_signin_fullscreen_signin_FullscreenSigninCoordinator.java.patch
  14-chrome_browser_ui_android_signin_java_src_org_chromium_chrome_ # Null-guard mediator
    browser_ui_signin_history_sync_HistorySyncCoordinator.java.patch
  15-chrome_browser_ui_android_toolbar_java_src_org_chromium_chrome_# isShown() = false
    browser_toolbar_signin_button_SigninButtonCoordinator.java.patch
  16-chrome_android_java_src_org_chromium_chrome_browser_sync_      # Hide sign-in toggle
    settings_GoogleServicesSettings.java.patch
```

### Patch 1: BUILD.gn — Register custom modules

**File:** `chrome/browser/BUILD.gn`
**Change:** Add 2 dependency lines

```gn
# Before:
    "//chrome/browser/navigation_predictor",
# After:
    "//chrome/browser/navigation_predictor",
    "//chrome/browser/navigation_policy:shorts_reels_blocker",
    "//chrome/browser/content_injection:content_injection",
    "//chrome/browser/navigation_predictor:impl",
```

**Why it works:** Chromium's build system uses recursive GN targets. Adding a `deps` entry in the parent `BUILD.gn` causes the source_set to be compiled and linked into `libchrome.so`. Without this, our custom modules are compiled but never linked.

### Patch 2: Navigation throttle registration — 2 lines

**File:** `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`
**Change:** One `#include` + one function call

```cpp
// Diff (exactly 2 lines added):

#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
// ...in the includes section, sorted alphabetically

// Inside CreateAndAddChromeThrottlesForNavigation():
    page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);
+   distraction_blocker::RegisterThrottlesAndHelpers(registry, handle);
```

**Why it's only 2 lines:** We consolidated 3 registrations (throttle + tab helper + injection manager) into a single C++ wrapper function. The wrapper lives in our `source-files/` module, so it never changes across versions. The upstream file only needs 2 lines of diff instead of 16+.

### Patch 3: NTP XML layout — Silent ViewStub Hiding

**File:** `chrome/android/java/res/layout/new_tab_page_layout.xml`
**Change:** 5 ViewStubs set to `gone` + `0dp`

Each ViewStub change follows the same pattern:

```xml
<!-- BEFORE: visible, takes space, inflates content -->
<ViewStub
    android:id="@+id/mv_tiles_layout_stub"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:layout_marginTop="@dimen/ntp_section_top_margin"
    android:layout_marginBottom="@dimen/ntp_section_bottom_margin"
    android:layout_marginLeft="@dimen/mvt_container_lateral_margin"
    android:layout_marginRight="@dimen/mvt_container_lateral_margin"
    android:layout="@layout/mv_tiles_layout" />

<!-- AFTER: invisible, zero size, R.id constant preserved -->
<ViewStub
    android:id="@+id/mv_tiles_layout_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/mv_tiles_layout"
    tools:visibility="gone" />
```

**The 5 hidden ViewStubs:**

| Layout ID | What it was | Why hide |
|---|---|---|
| `composeplate_view_stub` | AI Mode / Composeplate | Useless without sign-in |
| `mv_tiles_layout_stub` | Most Visited Tiles | We want clean NTP |
| `signin_promo_view_container_stub` | NTP sign-in promo | No sign-in |
| `home_modules_recycler_view_stub` | Discover feed / home modules | Distraction-free NTP |
| `tab_switcher_module_container_stub` | Recent tab card | Distraction-free NTP |

**THIS IS THE CRITICAL PATTERN.** Do not delete ViewStubs. Keep them, hide them. The `R.id` constant survives compilation. Java code compiles fine. The ViewStubs are never inflated at runtime because they're GONE with zero dimensions.

### Patch 4: NTP Coordinator — 4 initializations skipped

**File:** `chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPageCoordinator.java`
**Change:** Comment out 4 initialization blocks

```java
// In onFirstLayout(), after the original code sets up search box:

// SKIPPED: Most Visited Tiles removed from NTP (v24)
// initializeMostVisitedTilesCoordinator(
//         mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);

// SKIPPED: Composeplate removed from NTP (v24)
mIsComposeplateEnabled = false;
// initializeComposeplateFlags(mProfile);
// mNtpSearchBox.setIsFuseboxEligible(Boolean.TRUE.equals(mIsComposeplateEnabled));
// if (assumeNonNull(mIsComposeplateEnabled)) {
//     initializeComposeplate();
// }

// SKIPPED: Home modules removed from NTP (v24)
// initializeHomeModules();

// SKIPPED: Sign-in promo removed from NTP (v24)
// if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
//     initializeSigninPromoCoordinator();
// }
```

**The `mIsComposeplateEnabled = false` line is essential.** Without it, `assumeNonNull()` throws a NullPointerException because the field is never initialized by the skipped `initializeComposeplateFlags()` call. This is the null-guard pattern that every skipped initialization needs.

### Patch 5: NTP Java — Feed surface replaced with stub

**File:** `chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPage.java`
**Change:** Replace the 60-line FeedSurfaceCoordinator construction with a no-op stub

```java
// The original code creates a massive FeedSurfaceCoordinator with feed action
// delegates, snap scroll helpers, privacy prefs, swipe refresh, etc.
// We replace it with a lightweight stub:

// SKIPPED: Feed surface entirely removed from NTP.
// Create a plain FrameLayout wrapping the NTP layout instead.
FrameLayout rootView = new FrameLayout(activity);
rootView.setLayoutParams(new FrameLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT));
rootView.addView(mNewTabPageLayout);
final UiConfig ntpUiConfig = new UiConfig(rootView);
mFeedSurfaceProvider = new FeedSurfaceProvider() {
    @Override public void destroy() {}
    @Override public TouchEnabledDelegate getTouchEnabledDelegate() {
        return (enabled) -> {};
    }
    @Override public FeedSurfaceScrollDelegate getScrollDelegate() {
        return new FeedSurfaceScrollDelegate() {
            @Override public boolean isScrollViewInitialized() { return false; }
            @Override public boolean isChildVisibleAtPosition(int position) { return false; }
            @Override public int getVerticalScrollOffset() { return 0; }
            @Override public void snapScroll() {}
        };
    }
    @Override public UiConfig getUiConfig() { return ntpUiConfig; }
    @Override public View getView() { return rootView; }
    @Override public boolean shouldCaptureThumbnail() { return false; }
    @Override public void captureThumbnail(Canvas canvas) {}
    @Override public @Nullable FeedReliabilityLogger getReliabilityLogger() { return null; }
    @Override public void reload() {}
    @Override public NonNullObservableSupplier<Integer> getRestoringStateSupplier() {
        return ObservableSuppliers.createNonNull(
                FeedSurfaceProvider.RestoringState.NO_STATE_TO_RESTORE);
    }
    @Override public List<String> getFeedUrls() { return List.of(); }
};
```

**Why a stub and not null?** Because `mFeedSurfaceProvider` is used in `shouldCaptureThumbnail()`, `getView()`, and other places. If it's null, those methods NPE. The stub pattern returns safe default values for every method.

### Patch 6: NTP Layout Java — Remove site section initialization

**File:** `chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPageLayout.java`
**Change:** Remove the `initializeSiteSectionView()` call and its method body

```java
// Original onFinishInflate():
@Override
protected void onFinishInflate() {
    super.onFinishInflate();
    setBackgroundColor(getResources().getColor(
            R.color.home_surface_background_color, getContext().getTheme()));

    // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
    Log.i(TAG, "NewTabPageLayout.onFinishInflate before insertSiteSectionView");
    initializeSiteSectionView();  // ← THIS IS REMOVED
    Log.i(TAG, "NewTabPageLayout.onFinishInflate after insertSiteSectionView");
}

// Before: initializeSiteSectionView inflates the MV tiles stub
private void initializeSiteSectionView() {
    var mvTilesContainerLayout =
            (ViewGroup) ((ViewStub) findViewById(R.id.mv_tiles_layout_stub)).inflate();
    mvTilesContainerLayout.setVisibility(View.VISIBLE);
    if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
}
```

**Why we commented out `initializeSiteSectionView()` instead of just relying on the XML hiding:** The Java code calls `findViewById(R.id.mv_tiles_layout_stub).inflate()` which *inflates* the ViewStub even if it's hidden in XML. XML `visibility="gone"` only prevents the *initial display* of the stub's placeholder. Once you call `.inflate()`, the content renders regardless. So we must prevent the Java call.

### Patch 7: Settings Main — 200 lines removed (no sign-in, no passwords, no Google services)

**File:** `chrome/android/java/src/org/chromium/chrome/browser/settings/MainSettings.java`
**Change:** Remove sign-in pref, Google Services pref, autofill/passwords section, appearance, homepage conditionals

This is our largest patch (~200 lines diff). Key changes:

```java
// Sign-in preference: wrapped in null check so missing XML entry doesn't crash
SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
if (signInPreference != null) {
    // Entire sign-in setup block is conditional on the preference existing
}

// Google Services preference: null-guarded
ChromeBasePreference googleServicePreference = findPreference(PREF_GOOGLE_SERVICES);
if (googleServicePreference != null) {
    googleServicePreference.setViewId(R.id.account_management_google_services_row);
}

// Autofill & Passwords: entire section replaced with no-op
private void updateAutofillPreferences() {
    // Autofill & Passwords section removed from settings.
}
private void updateAutofillAndPasswords() {
    // Autofill & Passwords section removed from settings.
}
private void updateAutofillPreferencesPreAutofillAndPasswords() {
    // Autofill & Passwords section removed from settings.
}

// Sign-in promo card in updatePreferences():
// Promo card and sign-in preferences were removed from XML.
// Skip addPreferenceIfAbsent for these — they were never cached
// and would NPE inside assumeNonNull(mAllPreferences.get(key)).
```

**The critical insight:** When you remove a preference from XML but Java still calls `addPreferenceIfAbsent(key)`, it calls `findPreference(key)` which returns null if the key doesn't exist in the screen. Then `assumeNonNull()` in the Chromium helpers throws NPE. Solution: either remove the `addPreferenceIfAbsent` calls or wrap them in null checks.

### Patch 8: Settings XML — Remove Google account section

**File:** `chrome/android/java/res/xml/main_preferences.xml`
**Change:** Remove 7 preference entries

The removed entries:

```
1. SettingsPromoCardPreference (key: "settings_promo_card")
2. PreferenceCategory "account_and_google_services_section"
3. SignInPreference (key: "sign_in")
4. ChromeBasePreference "google_services" (key: "google_services")
5. ChromeBasePreference "autofill_and_passwords" (key: "autofill_and_passwords")
6. PreferenceCategory "autofill_section"
7-10. passwords, autofill_payment_methods, autofill_addresses, autofill_options
11. Preference "toolbar_shortcut" (key: "toolbar_shortcut")
12. Preference "ui_theme" (key: "ui_theme")
```

### Patch 9: Content settings — Notifications default deny

**File:** `components/content_settings/core/browser/content_settings_registry.cc`
**Change:** 2 lines changed

```cpp
// Before:
Register(ContentSettingsType::NOTIFICATIONS, "notifications",
         CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
         {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK}, ...);

// After:
Register(ContentSettingsType::NOTIFICATIONS, "notifications",
         CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
         {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK}, ...);
```

**Two changes:**
1. `CONTENT_SETTING_ASK` → `CONTENT_SETTING_BLOCK`: Default permission is now "blocked" instead of "ask"
2. Removed `CONTENT_SETTING_ASK` from valid settings: Users can only choose Allow or Block, not Ask

**Runtime effect:** Every new notification permission request is auto-denied. No prompt is shown. The site gets `PermissionDenied` without any UI. Chrome never asks "Allow notifications?" again.

### Patch 10: Sign-in default false

**File:** `components/signin/internal/identity_manager/primary_account_manager.cc`
**Change:** 1 value changed

```cpp
// Before:
registry->RegisterBooleanPref(prefs::kSigninAllowed, true);
// After:
registry->RegisterBooleanPref(prefs::kSigninAllowed, false);
```

**This is the nuclear option.** By default, sign-in is not allowed in the browser. No sign-in prompts, no account picker, no "Continue as..." dialogs. Combined with the UI patches, there is zero path to a signed-in state.

### Patch 11: Identity Disc hidden

**File:** `chrome/android/java/src/org/chromium/chrome/browser/identity_disc/IdentityDiscController.java`
**Change:** `get()` returns `canShow=false`, `setProfile()` skips all observer setup

The Identity Disc is the profile picture / avatar icon in the toolbar. Our patch:

```java
// Before: complex logic checking NTP state, account state, sign-in coordinator
@Override
public ButtonData get(@Nullable Tab tab) {
    mIsTabNtp = tab != null && tab.getNativePage() instanceof NewTabPage;
    if (!mIsTabNtp) {
        mButtonData.setCanShow(false);
        return mButtonData;
    }
    calculateButtonData();
    return mButtonData;
}

// After: always hidden
@Override
public ButtonData get(@Nullable Tab tab) {
    // The Identity Disc is intentionally always hidden in this distraction-free
    // fork. Google sign-in is not supported, so the button serves no purpose.
    mButtonData.setCanShow(false);
    return mButtonData;
}
```

`setProfile()` is similarly gutted — no IdentityManager observer registration, no SyncService listener, no ProfileDataCache construction. We clean up old observers (defensive) but register nothing new.

### Patch 12: Sign-in promo canShowPromo = false

**File:** `chrome/browser/ui/android/signin/java/src/org/chromium/chrome/browser/ui/signin/signin_promo/SigninPromoMediator.java`
**Change:** 1 function body replaced

```java
// BEFORE (12 lines of account-fetch logic + impression tracking):
boolean canShowPromo() {
    if (!mAccountManagerFacade.getAccounts().isFulfilled()
            || !mAccountManagerFacade.didAccountFetchSucceed()) {
        return false;
    }
    return !mMaxImpressionReached && mPromoDelegate.canShowPromo();
}

// AFTER (1 line):
boolean canShowPromo() {
    return false;
}
```

**This is the single greatest example of the minimal patch philosophy.** Our first version of this patch was 222 lines (changed method signatures, rewired constructors, removed imports). It broke on the stable branch because API signatures were different. The final fix: one line.

### Patch 13: Fullscreen sign-in — skip to next page

**File:** `chrome/browser/ui/android/signin/java/src/org/chromium/chrome/browser/ui/signin/fullscreen_signin/FullscreenSigninCoordinator.java`
**Change:** 2 lines

```java
// BEFORE:
public void continueSignIn() {
    mMediator.proceedWithSignIn();
}
public void cancelSignInAndDismiss() {
    mMediator.dismiss();
}

// AFTER:
public void continueSignIn() {
    mDelegate.advanceToNextPage();  // Skip sign-in, continue setup
}
public void cancelSignInAndDismiss() {
    mDelegate.advanceToNextPage();  // Same — skip sign-in
}
```

Both sign-in and cancel now advance to the next page. This means the first-run experience never actually signs in, but doesn't get stuck on a sign-in screen.

### Patch 14: History sync — null-guarded mediator

**File:** `chrome/browser/ui/android/signin/java/src/org/chromium/chrome/browser/ui/signin/history_sync/HistorySyncCoordinator.java`
**Change:** Constructor creates no mediator, null-guard all accesses

```java
// Constructor — dismiss immediately, don't create mediator:
public HistorySyncCoordinator(...) {
    delegate.dismissHistorySync(false, false);
    mActivity = activity;
    mProfile = profile;
    mView = null;
    mMediator = null;
}

// Every method is null-guarded:
public void destroy() {
    setView(null, false);
    if (mMediator != null) {
        mMediator.destroy();
    }
}
public void declineAndDismiss() {
    if (mMediator != null) {
        mMediator.declineAndDismiss();
    }
}
```

### Patch 15: Sign-in button — isShown = false

**File:** `chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/signin_button/SigninButtonCoordinator.java`
**Change:** 1 line

```java
public boolean isShown() {
    return false;  // was: mModel.get(SigninButtonProperties.SHOULD_SHOW_ON_PAGE);
}
```

### Patch 16: Google Services — hide sign-in toggle

**File:** `chrome/android/java/src/org/chromium/chrome/browser/sync/settings/GoogleServicesSettings.java`
**Change:** 2 changes

```java
// 1. Allow sign-in switch is always hidden:
mAllowSignin.setVisible(false);

// 2. shouldShowAllowSignIn always returns false:
private static boolean shouldShowAllowSignIn(Profile profile) {
    return false;  // was: !profile.isChild();
}
```

---

## Chapter 6: Content Blocking — The Navigation Throttle

### Architecture overview

```
User taps a link
       │
       ▼
NavigationThrottle::WillStartRequest()
       │
       ├── URL matches blocked pattern? ──→ BLOCK_REQUEST (show error page)
       │
       ▼
Page loads, user interacts with SPA
       │
       ▼
pushState() / replaceState() fires
       │
       ▼
ShortsReelsBlockerTabHelper::DidFinishNavigation()
       │
       ├── URL matches blocked pattern? ──→ NavigateToBlockPage()
       │
       ▼
SPA loads, DOMContentLoaded fires
       │
       ▼
ContentInjectionManager::DOMContentLoaded()
       │
       ├── Domain matches injection rule? ──→ Inject CSS/JS in isolated world
```

### The throttle engine

Our navigation throttle lives in `source-files/chrome/browser/navigation_policy/shorts_reels_blocker.cc` (314 lines). Here's how it works:

```cpp
// ── Step 1: Create for navigation ──────────────────────────────
std::unique_ptr<ShortsReelsBlockerThrottle>
ShortsReelsBlockerThrottle::CreateForNavigation(
    content::NavigationThrottleRegistry& registry) {
  return base::WrapUnique(new ShortsReelsBlockerThrottle(registry));
}

// ── Step 2: Both navigation and redirect are checked ───────────
ThrottleCheckResult WillStartRequest() override {
  return CheckAndMaybeBlock(navigation_handle()->GetURL());
}
ThrottleCheckResult WillRedirectRequest() override {
  return CheckAndMaybeBlock(navigation_handle()->GetURL());
}

// ── Step 3: Check URL against all rules ────────────────────────
ThrottleCheckResult CheckURL(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  const std::string lower_path = base::ToLowerASCII(url.path());

  // Prefix rules — block by domain + path prefix
  for (const auto& rule : GetAllPrefixBlockRules()) {
    if (!url.DomainIs(rule.registrable_domain))
      continue;
    if (rule.path_prefix.empty() ||
        PathMatchesPrefix(lower_path, rule.path_prefix)) {
      return BLOCK_REQUEST;
    }
  }

  // Regex rules — only evaluated when domain matches
  const auto& regex_rules = GetAllRegexBlockRules();
  for (size_t i = 0; i < regex_rules.size(); ++i) {
    if (url.DomainIs(regex_rules[i].registrable_domain) &&
        MatchesRegexRule(i, regex_rules[i].pattern, lower_path)) {
      return BLOCK_REQUEST;
    }
  }
  return PROCEED;
}

// ── Step 4: Path matching at component boundary ────────────────
bool PathMatchesPrefix(std::string_view path, std::string_view prefix) {
  if (!base::StartsWith(path, prefix, base::CompareCase::SENSITIVE))
    return false;
  // Enforce component-boundary: "/shortsfilm" must NOT match "/shorts"
  return path.size() == prefix.size() || path[prefix.size()] == '/';
}

// ── Step 5: Generate block page with live counter ──────────────
ThrottleCheckResult BlockRequestWithPage(const GURL& url) {
  int new_count = g_block_count.fetch_add(1, std::memory_order_relaxed) + 1;
  std::string html = BuildBlockPageHTML(new_count);
  return ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT,
                             std::make_optional(std::move(html)));
}
```

### Platform rules — the per-platform design

Each platform gets its own `.cc` / `.h` pair in `platform_rules/`. This keeps patches independent — adding TikTok blocking doesn't touch YouTube code.

**YouTube (youtube_block_rules.cc):**

```cpp
// youtube.com — block the Shorts feed.
// /shorts/<id> URLs and the /shorts browse page are blocked.
// Rest of YouTube (search, watch, channels) is unaffected.
constexpr BlockRule kYouTubePrefixRules[] = {
    {"youtube.com", "/shorts"},
};

base::span<const BlockRule> GetYouTubePrefixRules() {
  return kYouTubePrefixRules;
}
```

**TikTok (tiktok_block_rules.cc):**

```cpp
// tiktok.com — block the entire domain.
// TikTok is pure short-form video feed with no "safe" sub-sections.
// Empty path_prefix matches every URL on the domain.
constexpr BlockRule kTikTokPrefixRules[] = {
    {"tiktok.com", ""},  // "" → entire domain
};
```

**Full platform list:**

| Platform | What's blocked | Path prefix | Notes |
|---|---|---|---|
| YouTube | `/shorts` feed and individual Shorts | `/shorts` | Regular YouTube works |
| Instagram | `/reels/` path | `/reels/` | Posts, stories unaffected |
| Facebook | `/reels/` and `/watch/` | `/reels/`, `/watch/` | Feed unaffected |
| TikTok | Entire domain | `""` (empty) | TikTok = 100% short video |
| Reddit | `/rpan` live streams | `/rpan` | Regular subreddits work |
| X/Twitter | Video timeline | regex | More precise matching |
| LinkedIn | `/video` feed | `/video` | Feed and profiles work |

### The aggregator pattern

```cpp
// all_block_rules.cc — aggregates all platform rules into two vectors

base::span<const BlockRule> GetAllPrefixBlockRules() {
  static const base::NoDestructor<std::vector<BlockRule>> kRules([] {
    std::vector<BlockRule> rules;
    Append(rules, GetYouTubePrefixRules());
    Append(rules, GetInstagramPrefixRules());
    Append(rules, GetFacebookPrefixRules());
    Append(rules, GetTikTokPrefixRules());
    Append(rules, GetRedditPrefixRules());
    Append(rules, GetXPrefixRules());
    Append(rules, GetLinkedInPrefixRules());
    return rules;
  }());
  return *kRules;
}
```

**To add a new platform:**
1. Create `platform_rules/<name>_block_rules.h` + `.cc`
2. Add to `platform_rules/BUILD.gn`
3. Add `#include` + `Append()` call in `all_block_rules.cc`
4. Build takes 30 seconds (only new files compiled)

### SPA handling via TabHelper

Single-page apps (YouTube, Instagram) navigate via `pushState()` without triggering a full navigation. The throttle never sees these. The TabHelper catches them:

```cpp
class ShortsReelsBlockerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ShortsReelsBlockerTabHelper> {

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Only committed main-frame navigations (SPA pushState counts)
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasComitted()) {
      return;
    }
    MaybeBlockURL(navigation_handle->GetURL());
  }

  void MaybeBlockURL(const GURL& url) {
    // Reuse the throttle's CheckURL — logic lives in one place
    if (ShortsReelsBlockerThrottle::CheckURL(url).action() != BLOCK_REQUEST)
      return;

    ShortsReelsBlockerThrottle::NavigateToBlockPage(web_contents());
  }
};
```

### The block page

When a URL is blocked, the user sees a red stop-sign icon with the message "This page was blocked. Short-form videos aren't available here. That was your call." and a counter showing total blocks this session.

The block counter is an atomic `std::atomic<int>` — thread-safe, no locks, but not persisted across browser restarts.

---

## Chapter 7: The WebUI Blocked Page

### chrome://distraction-blocked

Instead of using Chromium's error page framework (net_error), we created a custom WebUI page. This gives us:

- Full control over HTML/CSS/JS
- A live session block counter
- Dark mode support via `prefers-color-scheme`
- Chromium's native neterror look and feel

### Registration

```cpp
// distraction_blocked_ui.cc (135 lines)

namespace {

std::string BuildPageHTML(int block_count) {
  std::string counter_text = base::StringPrintf(
      "blocked %d time%s this session",
      block_count, block_count == 1 ? "" : "s");

  return base::StrCat({R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#fff">
<meta name="viewport" content="width=device-width, initial-scale=1.0, ...">
<title>Page Blocked</title>
<style>
  /* Full CSS matching Chromium's native neterror page */
  body {
    --background-color:#fff;
    --heading-color:var(--google-gray-900);
    --text-color:var(--google-gray-700);
    --error-code-color:var(--google-gray-700);
    --link-color:rgb(88,88,88);
    background:var(--background-color);
    color:var(--text-color);
    font-family:system-ui, sans-serif;
    word-wrap:break-word;
    margin:0;padding:0;
  }
  .icon-blocked {
    background-image:url("data:image/svg+xml,...");
  }
  @media(prefers-color-scheme:dark){
    body {
      --background-color:var(--google-gray-900);
      --heading-color:var(--google-gray-500);
      --text-color:var(--google-gray-500);
      --link-color:var(--google-blue-300);
    }
    .icon-blocked {
      background-image:url("data:image/svg+xml,...");
    }
  }
</style>
</head>
<body>
  <div class="interstitial-wrapper">
    <div class="icon icon-blocked"></div>
    <h1><span>This page was blocked</span></h1>
    <p>Short-form videos aren't available here.</p>
    <div class="error-code">)HTML",
    counter_text,
    R"HTML(</div>
  </div>
</body>
</html>)HTML"});
}

}  // namespace

DistractionBlockedUI::DistractionBlockedUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  CreateAndAddDistractionBlockedHTMLSource(Profile::FromWebUI(web_ui));
}
```

### The counter is dynamic

Every page load queries `ShortsReelsBlockerThrottle::GetBlockCount()` and embeds the number in the HTML:

```cpp
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
```

### Registering the WebUI

Our custom WebUI URL `chrome://distraction-blocked` is registered in:

1. `chrome/common/webui_url_constants.h` — URL constant
2. `chrome/browser/ui/webui/webui_util.cc` — Host mapping
3. `chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc` — Factory registration

---

## Chapter 8: JavaScript Content Injection for SPAs

### The problem with blocking alone

Navigation throttles block full-page navigations. But many social media sites use SPAs: you browse Instagram, click a Reel in the feed, and a modal opens via `pushState()` — no navigation, no throttle trigger. The feed stays but the Reel plays.

### The injection approach

Instead of blocking navigation (too aggressive), we inject CSS/JS into the page *after* it loads. The page renders, but the offending elements are hidden or removed.

```cpp
// ContentInjectionManager — a WebContentsObserver

void ContentInjectionManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;
  RunMatchingRules(render_frame_host,
                   render_frame_host->GetLastCommittedURL());
}

void ContentInjectionManager::RunMatchingRules(
    content::RenderFrameHost* frame, const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  const std::string lower_path = base::ToLowerASCII(url.path());

  for (const InjectionRule& rule : GetInjectionRules()) {
    if (!url.DomainIs(rule.registrable_domain))
      continue;
    if (!rule.path_prefix.empty() &&
        !base::StartsWith(lower_path, rule.path_prefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }

    if (rule.type == InjectionType::kCSS) {
      // CSS injection via data URI stylesheet link
      const std::string encoded = base::Base64Encode(rule.payload);
      script = base::UTF8ToUTF16(
          "(function(){"
          "var l=document.createElement('link');"
          "l.rel='stylesheet';"
          "l.href='data:text/css;base64," + encoded + "';"
          "document.head.appendChild(l);"
          "})();");
    } else {
      // JavaScript injection as-is
      script = base::UTF8ToUTF16(std::string(rule.payload));
    }

    frame->ExecuteJavaScriptInIsolatedWorld(
        script,
        base::NullCallback(),
        content::ISOLATED_WORLD_ID_CONTENT_END);
  }
}
```

### Why isolated world?

`ExecuteJavaScriptInIsolatedWorld()` runs our injection in Chromium's `ISOLATED_WORLD_ID_CONTENT_END` — the same world Chrome extensions use. This means:

- Page JavaScript cannot see or tamper with our injected code
- No risk of `innerHTML` XSS from CSS payloads (we use `document.createElement('link')`)
- The injected styles appear in DevTools as "user stylesheet" (editable by user)
- The page's own JS cannot detect our presence via `document.stylesheets`

### CSS injection technique

We encode CSS as Base64 and create a `<link rel="stylesheet">` element with a data URI:

```
CSS payload: "div[data-testid='like-button'] { display: none !important; }"
    → Base64: "ZGl2W2RhdGEt...=="
    → data: URI: "data:text/css;base64,ZGl2W2RhdGEt...=="
    → <link> element appended to document.head
```

This avoids all string escaping issues. The Base64 representation is safe in any JS context.

### Platform-specific injection rules

| Platform | Type | What it hides |
|---|---|---|
| YouTube | CSS | Shorts shelf on watch page |
| Instagram | CSS | Reels tab in navigation bar |
| Facebook | CSS | Reels sidebar, watch tab |
| Reddit | JS | RPAN live video sections |
| X/Twitter | CSS | Video tab in timeline |
| LinkedIn | CSS | Reels feed in main column |
| Pinterest | CSS | Idea pins / video pins |
| Tumblr | CSS | Video posts in dashboard |

---

## Chapter 9: The Patch System — Our Infrastructure

### Generating patches

```bash
# 1. Edit files inside the checkout
docker compose run --rm builder bash
vim /checkout/src/chrome/android/java/.../MyFile.java

# 2. Copy modified file to source-files/
cp /checkout/src/path/to/file /sf/path/to/file

# 3. Generate .patch file
docker compose run --rm \
  -v $(pwd)/patches:/patches \
  builder python3 /patches/generate-patches.py /checkout/src /patches
```

### The apply-patches.py script

```python
#!/usr/bin/env python3
"""Apply our custom patches to a fresh Chromium checkout."""

import subprocess, sys, shutil, os
from pathlib import Path

def apply_patches(checkout_dir, patches_dir):
    checkout = Path(checkout_dir)
    patches = Path(patches_dir)

    # Phase 1: Copy Type A files (new modules from source-files/)
    source_files = patches / "source-files"
    for filepath in source_files.rglob("*"):
        if filepath.is_file():
            rel_path = filepath.relative_to(source_files)
            dest = checkout / rel_path
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(filepath, dest)

    # Phase 2: Apply Type B files (.patch files)
    patch_dir = patches / "patches"
    for pf in sorted(patch_dir.glob("*.patch")):
        result = subprocess.run(
            ["git", "apply", "--ignore-whitespace", str(pf)],
            cwd=checkout,
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"FAILED: {pf.name}")
            print(result.stderr)
            sys.exit(1)
        print(f"  OK: {pf.name}")
```

### The separation rule (v25+)

After our consolidation work in v25, the separation is clean:

```
source-files/   = ONLY new files that don't exist upstream
                  (custom blockers, injection system, WebUI page)
patches/*.patch = ALL modifications to upstream files
                  (throttle registration, NTP stripping, sign-in disable)
```

DO NOT keep modified upstream files in `source-files/`. If a file has a `.patch` file, the `source-files/` copy is redundant and causes drift.

### C++ Wrapper consolidation

Our original approach added 3 registrations inline in `chrome_content_browser_client_navigation_throttles.cc`:

```cpp
// BEFORE (16 lines of diff):
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
#include "chrome/browser/content_injection/content_injection_manager.h"
// ... + 14 lines of inline registration code

// AFTER (2 lines of diff):
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
    distraction_blocker::RegisterThrottlesAndHelpers(registry, handle);
```

The wrapper lives in `shorts_reels_blocker.cc`:

```cpp
namespace distraction_blocker {
void RegisterThrottlesAndHelpers(
    content::NavigationThrottleRegistry& registry,
    content::NavigationHandle& handle) {
  registry.AddThrottle(
      ShortsReelsBlockerThrottle::CreateForNavigation(registry));

  content::WebContents* web_contents = handle.GetWebContents();
  if (web_contents) {
    ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
    content_injection::ContentInjectionManager::CreateForWebContents(
        web_contents);
  }
}
}  // namespace distraction_blocker
```

**Why this matters:** `chrome_content_browser_client_navigation_throttles.cc` changes every Chromium release. A 2-line diff merges cleanly 99% of the time. A 16-line diff with 3 `#include`s and inline code will conflict every time.

---

## Chapter 10: The Build Log — Every Error We Hit

### Infrastructure errors

**Error 1: "Failed to convert napi value" — Node.js ABI mismatch**

```
# When building DevTools frontend:
Error: Failed to convert napi value into v8 value
    at Object.<anonymous> (.../devtools-frontend/src/node_modules/...)
```

**Root cause:** The Docker image ships Node.js 24. DevTools frontend uses `@web/rollup-plugin-import-meta-assets` which has a native addon compiled for a specific Node.js ABI. Node 24 is new and the prebuilt `.node` binary doesn't exist.

**Fix: Install Node.js 20 LTS in the Docker image and symlink it:**

```dockerfile
# In Dockerfile, after the base packages:
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    ln -sf $(which node) /usr/local/bin/node20
```

Then in the Makefile, use `node20` for DevTools builds. (We actually symlinked `node` itself to Node 20.)

---

**Error 2: "Cannot find module '@rollup/rollup-linux-x64-gnu'"**

```
Error: Cannot find module '@rollup/rollup-linux-x64-gnu'
Require stack:
- .../devtools-frontend/src/node_modules/@rollup/plugin-commonjs/...
```

**Root cause:** When DevTools runs `npm install`, the platform-specific `@rollup/rollup-linux-x64-gnu` package is resolved via the `package-lock.json`. If the lockfile was generated on a different platform (e.g., macOS dev machine), the Linux binary won't be listed.

**Fix: Delete the stale lockfile and regenerate:**

```bash
cd checkout/src/third_party/devtools-frontend/src
rm package-lock.json
npm install --no-audit --no-fund
```

This regenerates `package-lock.json` with the correct platform-specific packages.

---

**Error 3: "FileNotFoundError: libcore_core.rlib.d" — Missing Rust depfiles**

```
FileNotFoundError: [Errno 2] No such file or directory:
  '.../local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib/libcore_core.rlib.d'
```

**Root cause:** An interrupted build created `.rlib` files but not their corresponding `.rlib.d` depfiles. Ninja expects a `.d` file alongside every `.rlib`.

**Fix: Create empty depfiles for every matching `.rlib`:**

```bash
cd /checkout/src/out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib
for f in *.rlib; do
  touch "${f}.d"
done
```

---

**Error 4: "Text file busy" — Two competing ninja processes**

```
ninja: error: rebuilding 'out/Default/build.ninja' - Text file busy
```

**Root cause:** A previous Docker container was killed mid-build, leaving a ninja process running in the container. When a new container starts, both try to write to the same build state file.

**Fix: Kill all stale containers by name:**

```bash
docker kill $(docker ps -q --filter name=chromium-android) 2>/dev/null
docker rm $(docker ps -aq --filter name=chromium-android) 2>/dev/null
```

Always do this before starting a new build session.

---

**Error 5: Google Source rate limiting**

```
Error: 429 Too Many Requests
gclient: rate limit exceeded. Waiting 60 seconds...
```

**Cause:** `gclient sync` with default parallelism hits Google's API rate limits.

**Fix: Reduce parallelism:**

```bash
gclient sync --revision src@refs/tags/149.0.7827.84 --jobs 1
```

---

**Error 6: Docker disk full**

```
docker: Error response from daemon: write /var/lib/docker/overlay2/...: no space left on device.
```

**Fix: Clean Docker build cache:**

```bash
docker system prune -a
# This reclaimed ~50 GB in our case.
```

For deeper cleaning on the host:

```bash
sudo journalctl --vacuum-size=500M
sudo apt-get clean
rm -rf ~/.cache/
```

---

### Code errors

**Error 7: "Missing and no known rule to make ic_address_24dp.xml"**

```
ninja: error: 
  '.../chrome/android/java/res/drawable/ic_address_24dp.xml',
  needed by '.../chrome_apk_templates__build_config_java.jar',
  missing and no known rule to make it
```

**Root cause:** We had a gni patch that removed entries from `chrome_java_resources.gni`. But one of the removed drawables was still referenced by Java code.

**Fix: Dead drawable entries removed from gni.** (This was a chasing-tail problem — we eventually deleted the entire gni patch and accepted 0 changes to that file.)

---

**Error 8: "no member named SUB_APPS_WITHOUT_PROMPTS"**

```
../../components/content_settings/core/browser/content_settings_registry.cc:854:36:
  error: no member named 'SUB_APPS_WITHOUT_PROMPTS' in 'ContentSettingsType'
```

**Root cause:** We added a new content settings type registration for `SUB_APPS_WITHOUT_PROMPTS`. This enum exists on `main` but not on stable `149`. The stable branch doesn't have this mojom constant.

**Fix: Remove the registration block for `SUB_APPS_WITHOUT_PROMPTS`.** Stick to only the changes that exist on the stable branch.

---

**Error 9: "cannot find symbol: R.id.composeplate_view_stub"**

```
error: cannot find symbol
  symbol:   variable composeplate_view_stub
  location: class org.chromium.chrome.R.id
```

**Root cause:** We deleted `composeplate_view_stub` from `new_tab_page_layout.xml`. The `R.java` compiler no longer generates `R.id.composeplate_view_stub`. But `NewTabPageCoordinator.java` calls `findViewById(R.id.composeplate_view_stub)` — which fails to compile even though the code path is dead.

**Fix: Silent ViewStub Hiding.** Never delete ViewStubs. Keep them in XML, set `visibility="gone"` + `0dp` dimensions, and skip the Java `inflate()` calls. The `R.id` constants survive.

---

**Error 10: "incompatible types: DisplayableProfileData cannot be converted to CoreAccountInfo"**

```
error: incompatible types: DisplayableProfileData cannot be converted to CoreAccountInfo
```

**Root cause:** Our original `SigninPromoMediator.java` patch changed method signatures—replacing `getVisibleAccount()` return type from `CoreAccountInfo` to `DisplayableProfileData`. The stable branch's `SigninPromoMediator` uses different base classes than main.

**Fix: Revert to clean stable file, apply a ONE-LINE change: `return false`.**

---

**Error 11: NullPointerException in `assumeNonNull(mIsComposeplateEnabled)`**

```
java.lang.NullPointerException: assumeNonNull(mIsComposeplateEnabled) is null
```

**Root cause:** We SKIPPed the call `initializeComposeplateFlags(mProfile)` which sets `mIsComposeplateEnabled`. The field stays `null`. Later, `assumeNonNull()` crashes.

**Fix: Explicitly set the field:**

```java
mIsComposeplateEnabled = false;
```

---

**Error 12: "assumeNonNull(mAllPreferences.get('settings_promo_card'))"**

```
java.lang.NullPointerException: parameter mAllPreferences.get("settings_promo_card") is null
```

**Root cause:** We removed `SettingsPromoCardPreference` from `main_preferences.xml`. But `updatePreferences()` still calls `addPreferenceIfAbsent(PREF_SETTINGS_PROMO_CARD)`. The preference is never cached in `mAllPreferences`, so `findPreference()` returns null, and `assumeNonNull()` crashes.

**Fix: Remove the `addPreferenceIfAbsent` calls for removed preferences.** Or wrap them in null checks.

---

### The 3-build kill pattern

This is the single most important rule we discovered:

**When the SAME file fails compilation 3+ times from DIFFERENT errors, STOP. Download the clean original file from the stable tag and start over.**

Files we killed and reverted in v24:

| File | # of errors | Cause |
|---|---|---|
| `NewTabPageCoordinator.java` | 10+ Java errors | API drift between main and stable |
| `new_tab_page_layout.xml` | 4 R.id errors | Deleted ViewStubs broke Java compilation |
| `SigninPromoMediator.java` | 5 type errors | Method signature change from main |

The recovery command:

```bash
curl -sL "https://raw.githubusercontent.com/chromium/chromium/refs/tags/149.0.7827.84/path/to/NewTabPage.java" \
  -o /tmp/clean.java
```

---

## Chapter 11: Workflow Evolution — v1 to v25

### Version history timeline

| Version | What changed | Build time | Key lesson |
|---|---|---|---|
| **v1** | First default build on `main` | 8 hours | The APK exists at `out/Default/apks/ChromePublic.apk` |
| **v2–v9** | Experimentation with different approaches | Various | Many approaches don't survive a full build |
| **v10** | First patches for NTP stripping | 4 hours | Need better patch management |
| **v11–v16** | Adding sign-in removal, content settings | Various | Dead code still needs R.id references to compile |
| **v17** | Quality pass: NPE fixes, null-guards | 2 hours | Review EVERY public method after nulling a field |
| **v18–v22** | Refining, adding more platforms | Various | One change at a time for debugging |
| **v23** | Content injection upgrade: CSS→JS | 3 hours | Isolated world injection is safer |
| **v24** | **Stable branch pivot** (THE BIG ONE) | **6 days, 15+ builds** | API drift is the enemy |
| **v25** | Consolidation: wrapper function, cleanup | 1 hour | Every touchpoint removed = one less future conflict |

### v24 in detail (the painful pivot)

We switched from `main` to stable `149.0.7827.84`. This was the most painful part of the project:

**Day 1:** Fetch stable source. Apply existing patches. They break immediately — different file paths, different Java API signatures, different mojom enums.

**Day 2:** Fix `NewTabPageLayout.java` — but `R.id` errors from deleted ViewStubs.

**Day 3:** Try Silent ViewStub Hiding. It works! But `NewTabPageCoordinator.java` still has 10 errors.

**Day 4:** Revert `NewTabPageCoordinator.java` to clean stable. Apply minimal changes: 4 SKIP comments + `mIsComposeplateEnabled = false`.

**Day 5:** Fix `SigninPromoMediator.java` — abandon 200-line patch, use 1-line fix. IdentityDiscController patch needs full rewrite.

**Day 6:** Build succeeds. APK works. Commit v24.

### v25: Consolidation

After v24 succeeded, we cleaned up:

1. **Removed Geist custom fonts.** They required entries in `chrome_java_resources.gni` (which changes per version), custom `styles.xml` entries, and 5+ files. Deleted everything. The browser uses system fonts.

2. **Created C++ wrapper function.** Moved 14 lines of inline throttle/TabHelper/injection registration into a single function call. Upstream diff: 16 lines → 2 lines.

3. **Cleaned source-files/.** Removed all stock file copies (they were redundant with .patch files). Only custom modules remain.

4. **Deleted gni patch.** After removing Geist fonts, the gni had zero changes. Removed the patch entirely.

5. **Build result:** 378 steps (down from 14,336 full), exit 0. APK hash `85358ab0`.

### The philosophy that emerged

1. **Minimal patches survive.** A 1-line `return false` beats a 200-line API migration.
2. **Custom modules > upstream patches.** New files in `source-files/` never have merge conflicts. Patches against upstream files always do.
3. **Hide, don't delete.** ViewStubs in XML? Keep them, hide them. Preferences in Settings? Keep XML key, skip the `addPreferenceIfAbsent()`.
4. **Wrapper every 3+ inline lines.** One function call in the upstream file = zero conflict risk.
5. **When in doubt, revert.** Three failed fixes on the same file? Abandon. Download clean original. Apply one change.
6. **Pre-flight checks before every build.** Grep for missing resources, API drift, and enum changes BEFORE starting a 6-hour build.

---

## Chapter 12: The Complete Cheat Sheet

### Every command in one place

```bash
# ═══════════════════════════════════════════════════════════════
# SETUP (one-time)
# ═══════════════════════════════════════════════════════════════

# Create workspace
mkdir -p ~/chromium-android && cd ~/chromium-android

# Clone patches repo
git clone https://github.com/hxri-nxrxyxn/chromium.git patches

# Copy build configuration
cp patches/build-config/Dockerfile .
cp patches/build-config/docker-compose.yml .
cp patches/build-config/Makefile .
cp patches/build-config/.gclient checkout/

# Clone depot_tools + build Docker image
make setup

# ═══════════════════════════════════════════════════════════════
# BUILD CYCLE
# ═══════════════════════════════════════════════════════════════

# Fetch source (once)
make fetch

# Install deps (once per fresh checkout)
make deps

# Apply patches (after fetching or updating)
python3 patches/apply-patches.py checkout/src patches/

# Configure build (once per checkout)
make configure

# Build APK
make build

# Serve APK for download
cd serve && python3 -m http.server 8080

# ═══════════════════════════════════════════════════════════════
# DEVELOPMENT
# ═══════════════════════════════════════════════════════════════

# Open shell inside container
make shell

# Copy modified file from checkout to source-files
docker compose run --rm \
  -v $(pwd)/patches/source-files:/sf \
  builder bash -c "\
    mkdir -p /sf/\$(dirname chrome/android/java/.../MyFile.java) && \
    cp /checkout/src/chrome/android/java/.../MyFile.java \
       /sf/chrome/android/java/.../MyFile.java"

# Generate .patch files
docker compose run --rm \
  -v $(pwd)/patches:/patches \
  builder python3 /patches/generate-patches.py /checkout/src /patches

# Commit and push patches
cd ~/chromium-android/patches
git add -A
git commit -m "v26: description of changes"
git push origin main

# ═══════════════════════════════════════════════════════════════
# MONITORING
# ═══════════════════════════════════════════════════════════════

# Check build progress
tail -5 build-logs/build.log

# Check if ninja is alive
docker ps | grep chromium-android

# Kill stale containers
docker kill $(docker ps -q --filter name=chromium-android) 2>/dev/null
docker rm $(docker ps -aq --filter name=chromium-android) 2>/dev/null

# ═══════════════════════════════════════════════════════════════
# RECOVERY
# ═══════════════════════════════════════════════════════════════

# Clean Docker disk (reclaims 30-50 GB)
docker system prune -a

# Fix Rust depfile errors
docker compose run --rm builder bash -c "
  cd /checkout/src/out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib
  for f in *.rlib; do touch \"\${f}.d\"; done
"

# Fix DevTools lockfile platform issue
docker compose run --rm builder bash -c "
  cd /checkout/src/third_party/devtools-frontend/src
  rm package-lock.json && npm install --no-audit --no-fund
"

# Download clean original file from stable tag for revert
curl -sL \
  "https://raw.githubusercontent.com/chromium/chromium/refs/tags/149.0.7827.84/chrome/android/java/.../File.java" \
  -o /tmp/clean.java

# ═══════════════════════════════════════════════════════════════
# PRE-FLIGHT CHECKS (before every build!)
# ═══════════════════════════════════════════════════════════════

# Check for XML resources in .gni but missing on disk
comm -23 \
  <(find checkout/src/chrome/android/java/res -name '*.xml' | sed 's|.*/chrome/android/||' | sort) \
  <(grep -oP '"[^"]+\.xml"' checkout/src/chrome/android/chrome_java_resources.gni | tr -d '"' | sort)

# Check for API drift (methods/enums that exist on main but not stable)
grep -rn 'GlicHelper\|STANDBY_NO_FOCUS\|SUB_APPS' checkout/src/ 2>/dev/null | head -20
```

### What to do when...

| Symptom | Likely cause | Fix |
|---|---|---|
| `Missing and no known rule to make it` for XML | Gni references missing file | Add missing XML or remove gni entry |
| `cannot find symbol: R.id.xxx` | Deleted ViewStub from XML | Silent ViewStub Hiding: keep + hide |
| `cannot find symbol: SomeClass` | API doesn't exist on this branch | Grep for it, remove the reference |
| `no member named XXX` in C++ | Mojom enum missing on stable | Remove the registration block |
| NPE in Settings | Removed XML pref but Java still accesses it | Remove `addPreferenceIfAbsent()` call |
| NPE in NTP | Skipped init but `assumeNonNull()` checks field | Set the field to safe default |
| `Text file busy` | Stale ninja process | Kill all chromium-android containers |
| `napi value` error | Node.js ABI mismatch | Use Node.js 20 LTS |
| `@rollup/rollup-linux-x64-gnu` missing | Stale platform lockfile | `rm package-lock.json && npm install` |
| `.rlib.d` FileNotFound | Interrupted Rust build | Create empty depfiles for each `.rlib` |
| Docker disk full | Build caches + old images | `docker system prune -a` |
| Build passes but feature still shows | Patch didn't actually change behavior | Verify the file in checkout, re-apply patches |

---

## Epilogue: Why This Way?

We chose every approach in this guide because we tried the other way and it broke.

**Docker isolation** because host-only builds leave toolchain trash everywhere. **Stable branches** because `main` changes faster than we can patch. **Standard .patch files** because `sed` on Java files creates cascading syntax errors. **Silent ViewStub Hiding** because deleting XML elements breaks Java compilation at the `R.java` level — even dead code must reference valid IDs. **Wrapper functions** because 2 lines of diff merge cleanly across version upgrades while 16 lines don't. **No custom fonts** because `chrome_java_resources.gni` changes every version and we don't need the headache.

Every lesson in this guide was earned through failure. The good news: if you follow this guide, you skip directly to what works.

### Statistics for the curious

| Metric | Value |
|---|---|
| Total build attempts | 25 |
| Total ninja steps across all builds | ~143,000 |
| Total build time (wall clock) | ~40 hours |
| Patch files created | 16 |
| Custom source files written | ~40 |
| Lines of C++ written | ~2,000 |
| Lines of Java patched | ~300 |
| Lines of XML changed | ~100 |
| Disk space at peak | 95 GB |
| Disk space after `docker system prune -a` | 79 GB |

---

*Built from 6 days of Chromium builds, v1 through v25.*
*Chromium 149.0.7827.84 stable, June 2026.*
