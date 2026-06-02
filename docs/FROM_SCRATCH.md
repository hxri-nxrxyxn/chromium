# From Zero to Custom Chromium APK

### A Complete Guide to Building Your Own Browser

**Author:** Experience from building custom Chromium for Android v1 → v25
**Target audience:** Anyone who knows basic coding but has never touched Chromium
**Goal:** You can build this yourself, even without AI assistance

---

## Table of Contents

1. [What Are We Building?](#1-what-are-we-building)
2. [The Chromium Codebase: First Contact](#2-the-chromium-codebase-first-contact)
3. [Setting Up the Build Environment](#3-setting-up-the-build-environment)
4. [The Build System: GN and Ninja](#4-the-build-system-gn-and-ninja)
5. [Understanding Chromium's Source Layout](#5-understanding-chromiums-source-layout)
6. [First Build: Getting the APK Out](#6-first-build-getting-the-apk-out)
7. [Our Patch System](#7-our-patch-system)
8. [Content Blocking: The C++ Way](#8-content-blocking-the-c-way)
9. [Stripping the New Tab Page](#9-stripping-the-new-tab-page)
10. [Removing Google Sign-in and Accounts](#10-removing-google-sign-in-and-accounts)
11. [Stable Branch vs Main Branch](#11-stable-branch-vs-main-branch)
12. [C++ Consolidation: Wrapper Functions](#12-c-consolidation-wrapper-functions)
13. [The Build Log: Every Error We Hit](#13-the-build-log-every-error-we-hit)
14. [Workflow Evolution: v1 to v25](#14-workflow-evolution-v1-to-v25)
15. [The Complete Cheat Sheet](#15-the-complete-cheat-sheet)

---

## 1. What Are We Building?

We're building a custom version of Google Chrome for Android. Not a lightweight wrapper or WebView — a full Chromium APK that you can install on any Android phone.

**Our modifications:**
- Block short-form video content (YouTube Shorts, Instagram Reels, TikTok)
- Strip the New Tab Page to only a search bar (no news feed, no tiles, no AI mode)
- Remove all Google sign-in surfaces
- Default-deny all notification permissions
- Remove Google Password Manager from Settings
- Use system default fonts (no custom fonts needed)

**The constraint:** We must use the *stable* branch of Chromium (not the latest `main`), because building against `main` creates a new firehose of API changes every week.

**How long it takes:** First build: 6-10 hours (download + compile). Incremental builds after changes: 1-60 minutes depending on what changed.

---

## 2. The Chromium Codebase: First Contact

### What you're dealing with

The Chromium source repository is about **30 GB** when fully checked out. It contains:

- **10,000+ C++ files** (the browser engine)
- **50,000+ Java files** (Android UI)
- **300,000+ resource files** (XML layouts, images, strings)
- **1.2 million+ Git commits**

You don't need to understand all of it. You need to understand *which parts matter* and *how to find them*.

### The files that matter for Android modification

```
src/
├── chrome/
│   ├── android/                    ← MOST OF OUR WORK IS HERE
│   │   ├── java/src/org/chromium/chrome/browser/
│   │   │   ├── ntp/                ← New Tab Page (feed, tiles, search bar)
│   │   │   ├── settings/           ← Settings screens
│   │   │   ├── sync/               ← Sync and Google account settings
│   │   │   └── identity_disc/      ← The profile button in toolbar
│   │   ├── java/res/layout/        ← XML layouts for Android UI
│   │   ├── java/res/values/        ← Colors, styles, strings
│   │   └── chrome_java_resources.gni  ← Resource index (ADD everything here)
│   ├── browser/
│   │   ├── navigation_policy/      ← ← OUR CUSTOM FOLDER
│   │   ├── content_injection/      ← ← OUR CUSTOM FOLDER
│   │   ├── ui/webui/distraction_blocked/  ← ← OUR CUSTOM FOLDER
│   │   └── chrome_content_browser_client_navigation_throttles.cc  ← Throttle registration (WE PATCH THIS)
│   └── common/webui_url_constants.h  ← WebUI URL constants
├── components/
│   ├── content_settings/core/browser/content_settings_registry.cc  ← Default permissions
│   ├── signin/internal/identity_manager/primary_account_manager.cc  ← Sign-in core
│   └── signin/public/base/signin_pref_names.cc  ← Sign-in preference names
├── content/public/browser/
│   ├── navigation_throttle.h       ← Base class for our blocker
│   └── navigation_throttle_registry.h  ← Where throttles register
└── out/Default/                    ← Build output (not tracked in git)
    └── apks/                       ← ← YOUR APK LIVES HERE
```

### How to find what you need

**Finding UI code:** See a UI element in the browser and want to know its source? Search for `R.id.<element_name>` or `R.layout.<layout_name>`.

```bash
# Find which Java file uses a specific layout
grep -rn "R.layout.my_layout" chrome/android/java/src/

# Find where a specific UI string comes from
grep -rn "my_string_key" chrome/android/java/res/values/
```

**Finding C++ behavior:** Want to change how navigation works? Search for the throttle class you want to modify.

**The filename rule:** Chromium file paths match the namespace structure. `chrome/browser/navigation_policy/shorts_reels_blocker.h` means the namespace `chrome::browser::navigation_policy`.

---

## 3. Setting Up the Build Environment

### Why Docker?

Chromium has very specific tooling requirements:
- Depot tools (Google's build helpers)
- JDK 17 (not 11, not 21)
- Python 3.8+ 
- Node.js 20 LTS (the bundled Node 24 has issues)
- 16+ GB RAM
- 150+ GB free disk

Docker wraps all of this in a container so you never pollute your host machine. Everything lives in one directory: `~/chromium-android/`. To remove everything: `rm -rf ~/chromium-android/`.

### The directory structure

```
~/chromium-android/
├── Dockerfile              # Defines the build container
├── docker-compose.yml      # Mounts, ports, container config
├── Makefile                # One-command workflow
├── build-logs/             # Build output logs
├── depot_tools/            # Google's build tools (cloned once)
├── checkout/               # Chromium source (30GB+)
│   ├── .gclient            # Depot tools config
│   └── src/                # ← THE ACTUAL SOURCE
│       └── out/Default/    # Build output + APK
├── serve/                  # HTTP server for APK download
└── patches/                # ← OUR CUSTOM CHANGES (git repo)
    ├── source-files/       # Our custom C++/WebUI modules
    ├── patches/            # .patch files for stock Chromium files
    ├── build-config/       # Dockerfile, Makefile, etc.
    └── skills/             # This documentation
```

### Setting up step by step

```bash
# 1. Create the workspace
mkdir ~/chromium-android && cd ~/chromium-android

# 2. Get the build configuration
git clone https://github.com/hxri-nxrxyxn/chromium.git patches

# 3. Copy build files to workspace
cp patches/build-config/Dockerfile .
cp patches/build-config/docker-compose.yml .
cp patches/build-config/Makefile .
cp patches/build-config/args.gn checkout/src/out/Default/args.gn
cp patches/build-config/.gclient checkout/

# 4. Build the Docker image
docker compose build

# 5. Fetch the source (6-10 hours on first run!)
make fetch

# 6. Apply our patches
make patches

# 7. Configure build
make configure

# 8. Build the APK
make build
```

### Why these specific tools?

| Tool | Purpose | Alternative |
|------|---------|-------------|
| **Docker** | Isolated build environment | Native setup (messy, hard to clean) |
| **depot_tools** | Google's build scripts (gclient, gn) | No alternative — Chromium requires it |
| **GN** | Generates Ninja build files | Chromium's standard build system |
| **Ninja** | Actually compiles the code | Chromium's standard (parallel builds) |

---

## 4. The Build System: GN and Ninja

### Two-phase compilation

Chromium builds in two phases:

**Phase 1: `gn gen`** — The meta-build. Reads `.gni` (GN import) files and generates Ninja build files. This takes ~5 minutes. You run this when:
- You add new source files
- You change build flags
- You modify `BUILD.gn` or `.gni` files

**Phase 2: `ninja`** — The actual compilation. Compiles all changed files and produces the APK. This takes 10 minutes to 10 hours depending on what changed.

### Understanding GN targets

```gn
# A static_library compiles .cc files into a .a archive
static_library("browser") {
  sources = [
    "my_file.cc",
    "another_file.cc",
  ]
  deps = [
    "//chrome/browser/some_dependency",
    "//base",
  ]
}

# An android_apk packages everything into the final APK
android_apk("chrome_public_apk") {
  deps = [
    ":chrome_java",
    "//chrome/browser:navigation_policy",  # Our code
  ]
}
```

The `//` prefix means the path is relative to `src/` root. So `//chrome/browser:shorts_reels_blocker` means `src/chrome/browser/BUILD.gn` target `shorts_reels_blocker`.

### Resource compilation on Android

Android resources (layouts, images, strings) must be listed in resource index files. The main one is:

```
chrome/android/chrome_java_resources.gni
```

This file lists every XML file under `chrome/android/java/res/`. If you add a new XML file and don't list it here, it won't be compiled into the APK. If you add an entry for a file that doesn't exist, the build crashes.

### The APK packaging pipeline

```
Java files (.java)
    → javac compilation
    → .class files
    → dex (Dalvik Executable)
    → classes.dex
              \
XML resources   →  aapt2 packaging  →  .ap_  →  apkbuilder  →  .apk
C++ code (.cc)  →  clang compilation  →  .o  →  linker  →  libchrome.so
                                                              /
                                        (JNI, signing, zipalign)
```

---

## 5. Understanding Chromium's Source Layout

### The three layers

**Layer 1: `//content/`** — The "content module." Renders web pages, handles navigation, manages processes. This is the web engine layer. You rarely touch this unless you're modifying how pages load.

**Layer 2: `//chrome/browser/`** — The browser UI layer. Tabs, toolbars, menus, settings, sign-in. THIS is where most of our modifications live.

**Layer 3: `//chrome/android/`** — Android-specific Java code. Activities, fragments, layouts, Android resources. This is where NTP stripping and sign-in surface stripping happens.

### The pattern: Mirror the source tree

When we add custom files, we mirror the Chromium source tree layout:

```
chromium layout:                our custom files:
chrome/browser/                  patches/source-files/chrome/browser/
chrome/browser/navigation_policy/  patches/source-files/chrome/browser/navigation_policy/
```

This way our code integrates naturally with the build system — we just add our directory as a dependency in the parent `BUILD.gn`.

### Finding the right file to modify

**Scenario A: I want to change a UI text**
1. Open the browser on your phone
2. See the text you want to change
3. Search for it in `chrome/android/java/res/values/strings.xml`
4. The string key tells you which Java code uses it

**Scenario B: I want to remove a menu item**
1. Search for the menu title text in `strings.xml`
2. Find the string key (e.g., `settings_sign_in`)
3. Search for that key in Java files
4. You'll find where the menu item is added
5. Comment out the `addPreference()` call

**Scenario C: I want to modify how navigation works**
1. Decide when you want to block (before request? after response?)
2. Search for existing navigation throttle examples
3. Create a class that extends `content::NavigationThrottle`
4. Register it in `chrome_content_browser_client_navigation_throttles.cc`

---

## 6. First Build: Getting the APK Out

### Running your first build

```bash
cd ~/chromium-android
make build
```

This runs `ninja -j3 -C out/Default chrome_public_apk`. The `-j3` means 3 parallel tasks (adjust to your CPU). The `-C out/Default` specifies the build output directory.

**During the build,** there's almost no visible progress because Docker buffers stdout. Monitor with:

```bash
# Check if ninja is still running
docker top $(docker ps --filter "ancestor=chromium-android-builder" -q) | grep ninja

# Check progress
stat -c "%y" checkout/src/out/Default/.ninja_log
```

**Build output location:**
```
checkout/src/out/Default/apks/ChromePublic.apk
```

**Serving the APK for download:**
```bash
cd ~/chromium-android/serve
python3 -m http.server 8080
# Download from: http://YOUR_IP:8080/ChromePublic.apk
```

### Understanding build output

```
[1/14336] CXX obj/chrome/browser/my_file.o
[14330/14336] ACTION //chrome/android:chrome_public_apk__create(//build/toolchain/android:android_clang_arm64)
```

- `CXX` = Compiling C++ file
- `ACTION` = Running a build script (dex, resource packaging, APK signing)
- `LINK` = Linking a shared library
- `SOLINK` = Producing the final `libchrome.so`

The final step is always `chrome_public_apk__create`. If you see this with no errors, you have a working APK.

### Full rebuild vs incremental

Full rebuild (58,000+ targets): happens when:
- You run `gn gen` after changing args.gn
- You delete `out/Default/`
- You change a widely-included header

Incremental build (50-500 targets): happens when:
- You change a single `.cc` file
- You change a Java file
- You add/remove a resource

**A full rebuild takes 6-10 hours. An incremental build takes 1-30 minutes.** To avoid full rebuilds, never delete all `.o` files. Delete only specific ones:

```bash
# Instead of:
rm -rf out/Default/

# Do:
docker compose run --rm builder bash -c "
  rm -f /checkout/src/out/Default/obj/chrome/browser/navigation_policy/*.o
"
```

---

## 7. Our Patch System

### The problem: How do we modify Chromium without touching upstream?

We can't make changes directly in the Chromium source directory because:
1. It's 30GB and not our git repo
2. We need to apply changes to new versions easily

### The solution: Two types of changes

**Type A: Brand new files (our custom modules)**
These are files that don't exist in upstream Chromium. We simply copy them into the source tree. Examples:
- `chrome/browser/navigation_policy/shorts_reels_blocker.h`
- `chrome/browser/content_injection/content_injection_manager.cc`
- `chrome/browser/ui/webui/distraction_blocked/`

These live in `patches/source-files/` and get copied verbatim.

**Type B: Modified upstream files (patches)**
These are files that DO exist in Chromium but need small changes. Instead of keeping the whole modified file, we create a `.patch` file that records only our changes. Examples:
- `chrome_content_browser_client_navigation_throttles.cc` (we add 2 lines)
- `NewTabPageCoordinator.java` (we comment out 5 initializations)
- `new_tab_page_layout.xml` (we hide 5 ViewStubs)

These live in `patches/patches/` as `.patch` files.

### How patch files work

A `.patch` file (also called a unified diff) shows what lines to add (+) and remove (-), with context lines around them:

```diff
--- a/original_file.java
++ b/modified_file.java
@@ -150,16 +150,8 @@
     boolean canShowPromo() {
-        if (!mAccountManagerFacade.getAccounts().isFulfilled()
-                || !mAccountManagerFacade.didAccountFetchSucceed()) {
-            return false;
-        }
-        return !mMaxImpressionReached && mPromoDelegate.canShowPromo();
+        return false;
     }
```

The `@@ -150,16 +150,8 @@` means: "In the original file starting at line 150, I deleted 16 lines and added 8 lines."

### generate-patches.py

This script creates `.patch` files. It needs TWO things:
1. A **clean** Chromium checkout (the original unmodified source at your target version)
2. Your modified versions in `patches/source-files/`

It diffs them and produces `.patch` files:

```bash
docker compose run --rm \
  -v $(pwd)/patches:/patches \
  builder python3 /patches/generate-patches.py /checkout/src /patches
```

**Important:** If the Chromium checkout's `.git/` directory was deleted, `generate-patches.py` won't work because it uses `git diff`. You'll need to download the original source files from GitHub to create patches manually.

### apply-patches.py

This applies changes to a fresh checkout:

```bash
cd ~/chromium-android
python3 patches/apply-patches.py checkout/src patches/
```

It does two things:
1. Copies all Type A files (new modules) into the checkout
2. Applies all Type B `.patch` files via `git apply`

### The separation rule (v25+)

After consolidation (v25), the separation is clean:

```
source-files/   = ONLY new files that don't exist upstream
                    (custom blockers, injection, WebUI)
patches/*.patch = ALL modifications to upstream files
                    (throttle registration, NTP stripping, sign-in disable)
```

DO NOT keep modified upstream files in `source-files/`. If a file has a `.patch` file covering it, the `source-files/` copy is redundant and will cause drift.

---

## 8. Content Blocking: The C++ Way

### The architecture

We have two layers of content blocking that work together:

**Layer 1: Navigation Throttle (blocks before the page loads)**
```
User clicks a link → NavigationThrottle checks URL
                                      ↓
                              Blocked? → Show error page
                                      ↓
                              Allowed? → Load normally
```

**Layer 2: SPA TabHelper (blocks after the page changes)**
```
User clicks on a SPA link → pushState() fires
                                     ↓
                            TabHelper detects navigation
                                     ↓
                            Blocked? → Navigate to block page
```

### How the NavigationThrottle works

```cpp
class ShortsReelsBlockerThrottle : public NavigationThrottle {
  ThrottleCheckResult WillStartRequest() override {
    // Called BEFORE any network request
    return CheckAndMaybeBlock(GetNavigationHandle().GetURL());
  }

  ThrottleCheckResult CheckAndMaybeBlock(const GURL& url) {
    if (IsBlockedURL(url)) {
      return BLOCK_REQUEST;  // Never sends the request
    }
    return PROCEED;
  }
};
```

Every navigation goes through `WillStartRequest()` and `WillRedirectRequest()`. We check the URL against a list of blocked patterns. If it matches, we return `BLOCK_REQUEST` and show a custom error page (`chrome://distraction-blocked`).

### How the TabHelper works

The TabHelper handles single-page app (SPA) navigations. SPAs use `pushState()` to change URLs without triggering a full navigation — so the throttle never sees them. The TabHelper watches `DidFinishNavigation()` and checks every committed URL.

### Platform rules architecture

Each platform gets its own file pair under `platform_rules/`:

```
platform_rules/
├── youtube_block_rules.cc     # YouTube Shorts blocking
├── instagram_block_rules.cc   # Instagram Reels blocking
├── facebook_block_rules.cc    # Facebook Reels/Watch blocking
├── tiktok_block_rules.cc      # TikTok root domain blocking
├── reddit_block_rules.cc      # Reddit Reels blocking
├── x_block_rules.cc           # X/Twitter Reels blocking
├── linkedin_block_rules.cc    # LinkedIn Reels blocking
├── all_block_rules.cc         # Aggregator (includes all platforms)
```

To add a new platform:
1. Create `<name>_block_rules.h` and `<name>_block_rules.cc`
2. Add them to `platform_rules/BUILD.gn`
3. Include the header in `all_block_rules.cc` and call the rule function

### Content injection system

Instead of blocking navigation (which is aggressive), content injection injects CSS or JavaScript into the page after it loads. This is gentler — the page loads, but the offending elements are hidden.

Rules are defined per platform, same pattern. The injection happens at `DOMContentLoaded` via a `WebContentsObserver`.

---

## 9. Stripping the New Tab Page

### The problem

The Chromium New Tab Page (NTP) has:
- A search bar (we want this ✓)
- The Google logo (we want this ✓)
- Most Visited Tiles (we don't want this ✗)
- Discover feed (we don't want this ✗)
- Sign-in promo (we don't want this ✗)
- AI Composeplate (we don't want this ✗)
- Home modules (we don't want this ✗)
- Tab switcher card (we don't want this ✗)

### Approach 1: Delete the ViewStubs (FAILED)

Our first attempt deleted the ViewStubs from the XML layout. This crashed the build because Java code referenced `R.id.composeplate_view_stub` and similar IDs. The compiler resolves ALL symbols — even for methods that are never called. If the XML doesn't define an ID, the Java code can't compile.

### Approach 2: Silent ViewStub Hiding (SOLUTION)

Keep every ViewStub in the XML. But hide them:

```xml
<!-- Before: visible, inflates content -->
<ViewStub
    android:id="@+id/composeplate_view_stub"
    android:layout_width="match_parent"
    android:layout_height="@dimen/composeplate_view_height"
    android:layout="@layout/composeplate_view_layout" />

<!-- After: hidden, R.id preserved -->
<ViewStub
    android:id="@+id/composeplate_view_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/composeplate_view_layout"
    tools:visibility="gone" />
```

This keeps the XML IDs alive so Java compiles. Then in Java, we comment out the initialization calls:

```java
// SKIPPED: Most Visited Tiles removed from NTP
// mNewTabPageLayout.findViewById(R.id.mv_tiles_layout_stub).inflate();
```

### The three-layer approach

| Layer | What we change | Risk |
|-------|---------------|------|
| **XML** | Set all non-essential ViewStubs to `visibility="gone"` + `0dp` | Zero — R.id constants preserved |
| **Java** | Comment-out initialization call sites (not method bodies) | Zero — no structural changes |
| **Runtime** | Set nullable fields to safe defaults | Zero — `mIsComposeplateEnabled = false` prevents NPE |

### Why this works (and why deleting fails)

When you delete a ViewStub from XML:
- `R.id` generator no longer creates the constant
- Java `findViewById(R.id.composeplate_view_stub)` fails compilation
- Even if the code never runs, it must compile ✓✓

When you keep the ViewStub but hide it:
- `R.id` constants exist
- Java compiles fine
- Views are not inflated (GONE ViewStubs are invisible)
- The browser shows only logo + search bar

---

## 10. Removing Google Sign-in and Accounts

### The surface areas

Google sign-in surfaces in Chromium for Android:

| Surface | File | How we disabled |
|---------|------|-----------------|
| NTP sign-in promo | `SigninPromoMediator.java` | `canShowPromo()` returns `false` |
| NTP sign-in promo coordinator | `NtpSigninPromoCoordinator.java` | Called from SKIPPed code |
| Settings "Sign in" entry | `MainSettings.java` | Removed `addPreferenceIfAbsent()` |
| Settings "Google Services" | `GoogleServicesSettings.java` | Removed entry |
| History sync opt-in | `HistorySyncCoordinator.java` | Null-guarded mediator |
| Full-screen sign-in | `FullscreenSigninCoordinator.java` | Called only from SKIPPed code |
| Toolbar sign-in button | `SigninButtonCoordinator.java` | Called only from SKIPPed code |
| Identity disc (profile icon) | `IdentityDiscController.java` | Disabled |
| Core C++ sign-in | `primary_account_manager.cc` | Always returns "not signed in" |

### The one-line rule

For `SigninPromoMediator.java`, our first patch was **222 lines** — it changed method signatures, rewired dependencies, and removed constructors. It broke because the stable branch has different API signatures.

The final fix was **one line**:

```java
boolean canShowPromo() {
    return false;  // Never show the sign-in promo
}
```

This pattern applies everywhere in Chromium: **methods that check "should I show this?" return `false`**, and the feature disappears without needing structural changes.

### The null-guard pattern

When you skip initialization in the constructor, some fields stay `null`. Any method that accesses those fields later will crash:

```java
// BAD: Constructor skips init, but destroy() still accesses the null field
public HistorySyncCoordinator() {
    delegate.dismissHistorySync(false, false);
    mMediator = null;  // 💣
}
public void destroy() {
    mMediator.declineAndDismiss();  // NPE at runtime!
}

// GOOD: Guard every access
public void destroy() {
    if (mMediator != null) {
        mMediator.declineAndDismiss();
    }
}
```

**The compiler won't catch this.** It compiles fine and crashes at runtime when you open Settings. Always grep for every reference to a nulled field.

---

## 11. Stable Branch vs Main Branch

### Why we switched

Building against `main` means every Chromium release (every 4 weeks) changes APIs you depend on. The NTP Java files are completely different between versions. Sign-in interfaces change. Resource files get added and removed.

Building against a **stable release tag** (like `149.0.7827.84`) means your patches target a fixed API surface. You only need to update when you want to move to the next stable version.

### How to pin a version

```makefile
CHROMIUM_VERSION ?= 149.0.7827.84
CHROMIUM_TAG ?= refs/tags/$(CHROMIUM_VERSION)
```

`gclient sync --revision src@refs/tags/$(CHROMIUM_VERSION)` pins the checkout to that specific tag.

### The cost of switching

When you switch from main to a stable branch, ALL your patches need to be re-tested. Files that existed on main may not exist on stable. Interfaces may have different signatures. The things that WILL break:

1. **Resource index drift** — XML files in the source tree but not in the `.gni` → add missing entries
2. **Mojo enum drift** — C++ enums added after the stable branch → remove those registration blocks
3. **Java API drift** — NTP customization classes different → comment-out the references

### Pre-flight checklist before every new version

After applying patches but BEFORE building:

```bash
# Check for missing XML resources
docker compose run --rm builder bash -c "
comm -23 <(find /checkout/src/chrome/android/java/res -name '*.xml' | sed 's|.*/chrome/android/||' | sort) \
         <(grep -oP '\"[^\"]+\\.xml\"' /checkout/src/chrome/android/chrome_java_resources.gni | tr -d '\"' | sort)
"

# Check for API drift in Java
docker compose run --rm builder bash -c "
grep -rn 'GlicHelper\\|STANDBY_NO_FOCUS\\|shouldShowHomepageSettings' /checkout/src/chrome/android/java/src/ 2>/dev/null
"

# Check for enum drift
docker compose run --rm builder bash -c "
grep -rn 'SUB_APPS\\|MAIN_ONLY_FEATURE' /checkout/src/components/content_settings/ 2>/dev/null
"
```

**Skipping this cost us 3 builds at 90%+ progress.** Each restart was a 1-line fix that could have been caught in 30 seconds of grepping.

---

## 12. C++ Consolidation: Wrapper Functions

### The problem

Our custom code needed to register THREE things in `chrome_content_browser_client_navigation_throttles.cc`:
1. `ShortsReelsBlockerThrottle`
2. `ShortsReelsBlockerTabHelper`
3. `ContentInjectionManager`

The original approach added 2 `#include` lines + a 14-line registration block. That's 16 lines of diff in a file that changes EVERY Chromium release. Each update meant merge conflicts in this file.

### The solution

Create a wrapper function in our custom module:

```cpp
// shorts_reels_blocker.h
namespace distraction_blocker {
void RegisterThrottlesAndHelpers(
    content::NavigationThrottleRegistry& registry,
    content::NavigationHandle& handle);
}

// shorts_reels_blocker.cc
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
}
```

Now the upstream file needs only:

```cpp
// In includes (added by patch):
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"

// In the registration function (added by patch):
distraction_blocker::RegisterThrottlesAndHelpers(registry, handle);
```

**Result:** 16 lines of diff → 2 lines of diff. Merge conflict probability: near zero.

### When to use this pattern

Use a wrapper when you're adding 3+ lines of inline code to a file that changes frequently. Don't use it for a single line — a single `#include` or a one-line flag change doesn't need a wrapper.

### Files that change every release

| File | How often it changes | Our diff size |
|------|-------------------|---------------|
| `chrome_content_browser_client_navigation_throttles.cc` | Every release | **2 lines** (was 16) |
| `chrome/browser/BUILD.gn` | Often | 3 lines (deps) |
| `chrome/android/chrome_java_resources.gni` | Every release | **0 lines** (deleted) |
| `new_tab_page_layout.xml` | Every release | 86 lines (hidden stubs) |

---

## 13. The Build Log: Every Error We Hit

### Infrastructure issues

| Error | Why it happened | How we fixed it |
|-------|----------------|-----------------|
| `Error: Failed to convert napi value` | Node.js 24 ABI mismatch with native rollup plugin | Installed Node.js 20 LTS in Docker |
| `Cannot find module '@rollup/rollup-linux-x64-gnu'` | Stale package-lock.json missing platform-specific deps | Deleted lockfile, regenerated with `npm install` |
| `FileNotFoundError: libcore_core.rlib.d` | 23 Rust depfiles missing after interrupted build | Created empty stub `.d` files matching `.rlib` names |
| `Permission denied` on checkout files | Docker creates root-owned files | Copy/edit from INSIDE the container |
| `Text file busy` on .o files | Two Docker containers running competing ninja | `docker kill` + `docker rm` stale containers |
| Google Source rate limiting | Too many concurrent requests | Retry with `--jobs 1` |
| Docker disk full (80+ GB) | Build caches + old images | `docker system prune -a` reclaimed 50+ GB |

### Patch/code issues

| Error | Why | Fix |
|-------|-----|-----|
| `Missing rule to make ic_address_24dp.xml` | Dead drawable entries in gni patch | Removed entries from gni |
| `no member named SUB_APPS_WITHOUT_PROMPTS` | Mojom enum from main doesn't exist on stable | Removed the registration block |
| `cannot find symbol: R.id.composeplate_view_stub` | Deleted ViewStub from XML but Java still needs it | Silent ViewStub Hiding (keep + hide) |
| `incompatible types: DisplayableProfileData cannot be converted to CoreAccountInfo` | Changed getVisibleAccount() return type unnecessarily | Reverted to original stable + 1-line change |
| NPE in `assumeNonNull(mIsComposeplateEnabled)` | Field never initialized after we SKIPPed its init | Added `mIsComposeplateEnabled = false` |
| `assumeNonNull(mAllPreferences.get("settings_promo_card"))` | Removed XML key but didn't remove addPreferenceIfAbsent | Removed the entire addPreferenceIfAbsent block |

### The 3-build kill pattern

This is the most important lesson:

When the SAME file fails compilation 3+ times from DIFFERENT errors, STOP. You're fighting the codebase instead of working with it. Download the clean original file from the stable tag and start over.

Files we killed in v24:
- `NewTabPageCoordinator.java` — 10+ Java errors → abandoned
- `new_tab_page_layout.xml` — 4 R.id errors → abandoned
- `SigninPromoMediator.java` — 5 type errors → replaced with 1-line fix

---

## 14. Workflow Evolution: v1 to v25

### v1: First build (main branch)

- Full default build: succeeded first try
- No custom changes yet
- Just learning the workflow
- **Time:** ~8 hours (download + compile)

### v10: Adding patches

- First patches for NTP stripping
- Content in `source-files/`copied verbatim
- Patch files generated manually
- **Lesson:** Need better patch management

### v17: Quality pass

- Discovered NPE bugs in sign-in skipping
- Added null-guards
- Code review checklist created
- **Lesson:** Review EVERY public method after nulling a field

### v23: Content injection upgrade

- CSS injection → JavaScript injection
- SPA-aware blocking via pushState/replaceState
- Per-platform rule files (refactored)
- **Lesson:** Single-purpose files survive version changes

### v24: Stable branch pivot (THE BIG ONE)

- Switched from main to 149.0.7827.84
- 15+ build attempts over 6 days
- Discovered API drift categories 1-9
- Silent ViewStub Hiding technique invented
- Minimal patch philosophy established
- **Lesson:** Patches against main don't work on stable. Target your branch from day one.

### v25: Consolidation

- Removed Geist custom fonts (unnecessary touchpoint)
- C++ wrapper function for throttle registration
- Source-files cleaned: only custom modules remain
- gni patch deleted (zero changes needed)
- 2-line diff in the most volatile upstream file
- **Lesson:** Every touchpoint you remove = one less merge conflict

### The philosophy that emerged

1. **Minimal patches survive.** A 1-line behavioral change beats a 200-line API migration.
2. **Custom modules > upstream patches.** New files in `source-files/` never have merge conflicts. Patches against upstream files always do.
3. **Hide, don't delete.** ViewStubs in XML? Keep them, hide them. Preferences in Settings? Keep the XML key, don't add it to screen.
4. **Wrapper every 3+ inline lines.** One function call in the upstream file = zero conflict risk.
5. **When in doubt, revert.** Three failed fixes? Abandon the file. Download clean original. Apply one change.

---

## 15. The Complete Cheat Sheet

### Every command you'll ever need

```bash
# ===== SETUP =====

# Create workspace
mkdir -p ~/chromium-android && cd ~/chromium-android

# Clone patches
git clone https://github.com/hxri-nxrxyxn/chromium.git patches

# Copy build config
cp patches/build-config/Dockerfile .
cp patches/build-config/docker-compose.yml .
cp patches/build-config/Makefile .
cp patches/build-config/.gclient checkout/

# Build Docker image
docker compose build

# Fetch source (6-10 hours)
make fetch

# ===== BUILDING =====

# Apply patches to checkout
cd ~/chromium-android
python3 patches/apply-patches.py checkout/src patches/

# Build APK
make build
# OR manually:
docker compose run --rm builder bash -c "
  cd /checkout/src && ninja -j3 -C out/Default chrome_public_apk
"

# Serve APK for download
cd ~/chromium-android/serve
python3 -m http.server 8080
# Download: http://YOUR_IP:8080/ChromePublic.apk

# ===== DEVELOPMENT =====

# Copy a modified file from checkout to source-files
docker compose run --rm \
  -v $(pwd)/patches/source-files:/sf \
  builder bash -c "
    mkdir -p /sf/\$(dirname path/to/file)
    cp /checkout/src/path/to/file /sf/path/to/file
  "

# Generate patches
docker compose run --rm \
  -v $(pwd)/patches:/patches \
  builder python3 /patches/generate-patches.py /checkout/src /patches

# Commit and push patches
cd ~/chromium-android/patches
git add -A
git commit -m "description"
git push origin main

# ===== MONITORING =====

# Check if build is alive
docker top $(docker ps --filter "ancestor=chromium-android-builder" -q) | grep ninja

# Check build progress
stat -c "%y" checkout/src/out/Default/.ninja_log

# Check for stale containers
docker ps --format '{{.Names}}' | grep builder

# Kill stale containers
docker kill $(docker ps -q --filter name=builder) 2>/dev/null
docker rm $(docker ps -aq --filter name=builder) 2>/dev/null

# ===== RECOVERY =====

# Clean Docker disk space (reclaims 50-80 GB)
docker system prune -a

# Nuke checkout from inside Docker
docker compose run --rm builder bash -c "rm -rf /checkout/* /checkout/.* 2>/dev/null"

# Fix Rust depfile errors
docker compose run --rm builder bash -c "
  cd /checkout/src/out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib
  find . -maxdepth 1 -name '*.rlib' -exec sh -c 'touch \"\${1}.d\"' _ {} \\;
"

# Fix DevTools lockfile
docker compose run --rm builder bash -c "
  cd /checkout/src/third_party/devtools-frontend/src
  rm package-lock.json && npm install --no-audit --no-fund
"

# ===== VERSION SWITCH =====

# Download a clean original from stable tag
curl -sL \"https://raw.githubusercontent.com/chromium/chromium/refs/tags/149.0.7827.84/path/to/file.java\" -o /tmp/clean_file.java

# Pre-flight checks before building on a new version
# (See section 11 above)
```

### What to do when...

**Build fails with "Missing and no known rule to make it" for an XML file:**
→ A .gni file references a resource that doesn't exist on disk. Either create the missing XML file or remove the reference from the .gni.

**Build fails with Java "cannot find symbol":**
→ The `R.java` file doesn't have the ID. Two causes:
  1. You deleted a ViewStub from XML → revert to original XML, use Silent ViewStub Hiding
  2. You're using an API that doesn't exist on this branch → grep for it, remove the reference

**Build fails with C++ "no member named ...":**
→ You're using a mojom enum or method that doesn't exist on this branch. Remove the registration block.

**Build passes but Settings crashes when opened:**
→ You removed a preference from `main_preferences.xml` but `updatePreferences()` still calls `addPreferenceIfAbsent()` for it. Remove the call.

**Build passes but NTP feed still shows:**
→ The ViewStub isn't hidden. Check `new_tab_page_layout.xml` — the `android:visibility="gone"` attribute must be on the correct ViewStub.

**Docker says "Container ... Creating" and hangs:**
→ A stale container from a previous run is holding the lock. Kill it with `docker kill $(docker ps -q --filter name=builder)` and retry.

**`git push` in patches/ takes forever:**
→ You're pushing from `checkout/src/` instead of `patches/`. The checkout directory has a 30GB+ artifacts. Always push from `~/chromium-android/patches/`.

---

## Epilogue: Why This Way?

We chose every approach in this guide because we tried the other way and it broke.

- **Docker isolation** because host-only builds left toolchain trash everywhere
- **Stable branches** because `main` changes faster than we can patch
- **Standard .patch files** because `sed` on Java files creates cascading syntax errors
- **Silent ViewStub Hiding** because deleting XML elements breaks Java compilation
- **Wrapper functions** because 2 lines of diff merge better than 16 lines
- **No custom fonts** because `chrome_java_resources.gni` changes every version and we don't need the headache

Every lesson in this guide was earned through failure. The good news: if you follow this guide, you skip directly to what works.

---

*Built and documented from 6 days of Chromium builds, v1 through v25.*  
*Last updated: June 2026, Chromium 149.0.7827.84 stable*
