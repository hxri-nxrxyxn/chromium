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

Build Chromium for Android (`chrome_public_apk`) inside a Docker container with all tooling isolated. Everything lives in one directory (`~/chromium-android/`).

Full content available at GitHub: https://github.com/hxri-nxrxyxn/chromium/blob/main/skills/chromium-development.md

NTP stripping reference: `references/stripping-ntp-android.md`
Silent ViewStub Hiding (v24+): `references/silent-viewstub-hiding.md`
Privacy & account hardening: `references/privacy-hardening.md`
Font integration (Geist): `references/font-integration.md`
Network & rate-limiting: `references/network-issues.md`
Git bloat recovery: `references/git-bloat-recovery.md`
Stable branch porting: `references/stable-branch-porting.md`

## ⚠️ Stale builder containers — check before starting ANY new command

Docker compose `run --rm` creates a container that persists until the process exits. If a previous command was killed (Ctrl+C, timeout), the container may still be running and holding the checkout volume lock. Two builder containers running simultaneously causes:

- Permission errors on write operations
- "Text file busy" on compiled objects
- Docker compose hanging on "Creating..." or "Container ... Creating"

**Before any builder command, verify:**

```bash
docker ps --format '{{.Names}}' | grep builder
# If any output: kill them
docker kill $(docker ps -q --filter name=builder) 2>/dev/null
docker rm $(docker ps -aq --filter name=builder) 2>/dev/null
```

**⚠️ Killing the outer process does NOT kill the Docker container.** When using `process kill` or Ctrl+C, the Docker container persists with its internal ninja processes still running. Multiple ninja instances competing for the same `out/Default` directory corrupt the build state. Always verify with `docker ps` after any kill, and use explicit `docker kill` + `docker rm`.

This is especially critical after a failed/interrupted build. The failed container may still be holding locks on `out/Default/` object files.

## ⚠️ Patched resource lists MUST have corresponding files

When a patch modifies a resource index file (like `chrome_java_resources.gni`), it may add entries for files that were never committed to `source-files/`. The build will fail with:

```
ninja: error: '../../chrome/android/java/res/drawable/<file>.xml', needed by '...resources.zip', missing and no known rule to make it
```

**After applying patches, verify all referenced resources exist:**

```bash
# Extract all resource paths from patched .gni files and check they exist
cd /checkout/src
grep -oP '"java/res/[^"]+' chrome/android/chrome_java_resources.gni | \
  tr -d '"' | while read f; do
  [ -f "$f" ] || echo "MISSING: $f"
done
```

Missing files need to be created (placeholder drawable, empty layout, etc.) or the .gni patch needs to be updated to remove the reference.

**Audit for dead entries:** Before regenerating patches, verify every resource added to .gni files is actually referenced by your Java/C++ code. Dead drawable entries (added speculatively but never backed with XML files or code) cause build failures on EVERY file. Check:

```bash
# For each resource your patch ADDS, verify either the file exists or code references it
grep -r "R.drawable.ic_spark" source-files/chrome/android/java/   # Java references?
ls source-files/chrome/android/java/res/drawable/ic_spark*.xml    # File exists?
```

If neither exists, remove the entry from the source gni BEFORE generating the patch. 14 dead drawable entries were stripped in v24.

This triggers a full 50k-target rebuild taking 6-10+ hours. Delete only specific .o files from INSIDE Docker:

```bash
docker run --rm -v ./checkout:/checkout alpine:latest sh -c "
  rm -f /checkout/src/out/Default/obj/chrome/browser/navigation_policy/*.o
"
```

## Privacy & Account Hardening

See `references/privacy-hardening.md` for:
- Sign-in surface stripping (Java coordinators)
- Settings backdoor audit (hiding "Allow Chrome sign-in" toggle)
- Notification permission default-deny (content_settings_registry.cc)
- C++ sign-in disabling (kSigninAllowed pref)

## NTP Stripping

See `references/stripping-ntp-android.md` for stripping NTP to search bar only.

## Pushing patches to GitHub

The patches directory (`patches/`) is its own **separate git repo** — it is NOT the Chromium checkout's `.git`. Always push from there:

```bash
cd /home/hari/chromium-android/patches
git add -A
git commit -m "describe the change"
git push origin main
```

**Critical:** Never push from inside `checkout/src/` — that's the 30GB+ Chromium checkout and will time out. The patches repo is small and pushes in seconds.

**What's tracked in the patches repo:**
- `source-files/` — all modified Chromium source files (mirrors `src/` layout)
- `apply-patches.py` / `generate-patches.py` — patch application and generation
- `try_snippets/` — standalone JS snippets (reference, not compiled)
- `build-config/` — Dockerfile, docker-compose, Makefile, args.gn, .gclient
- `skills/` — reusable workflows and references
- `README.md`, `INTEGRATION.md` — documentation

Patch files live under `source-files/` mirroring the Chromium source tree layout:
- `patches/source-files/chrome/browser/...`
- `patches/source-files/components/...`

After making changes in `checkout/src/`, sync to patches:

```bash
cd /home/hari/chromium-android/checkout/src
git diff --name-only HEAD | while read f; do
  mkdir -p "/home/hari/chromium-android/patches/source-files/$(dirname "$f")"
  cp "$f" "/home/hari/chromium-android/patches/source-files/$f"
done
```

Then commit & push from the patches repo.

**⚠️ Patches repo can diverge from checkout:** The GitHub repo (`hxri-nxrxyxn/chromium`) may have commits the local patches dir doesn't have (e.g. refactored platform rule files). Before reviewing or building, always do `git pull origin main` in the patches dir first, then sync to checkout.

## Code review from GitHub — always clone fresh

When the user asks you to review code, do NOT read from local patch files. Clone fresh from GitHub into `/tmp/`:

```bash
rm -rf /tmp/chromium-review && git clone https://github.com/hxri-nxrxyxn/chromium.git /tmp/chromium-review
```

This ensures you're reviewing what's actually on GitHub, not stale local copies. The local patches dir may be behind the remote.

## Platform-rules architecture (refactored pattern)

The codebase has been refactored from monolithic rule files to per-platform aggregation. There are two parallel systems:

### 1. Navigation Block Rules — `chrome/browser/navigation_policy/platform_rules/`

Each platform gets its own `.{h,cc}` pair (e.g. `youtube_block_rules.h`, `youtube_block_rules.cc`). The aggregator `all_block_rules.cc` pulls them all together:

```
platform_rules/
├── block_rule_types.h          # shared structs (BlockRule, RegexBlockRule)
├── all_block_rules.{h,cc}      # sole aggregator — includes all platforms
├── BUILD.gn                    # sources + deps
├── youtube_block_rules.{h,cc}
├── facebook_block_rules.{h,cc}
├── instagram_block_rules.{h,cc}
├── linkedin_block_rules.{h,cc}
├── reddit_block_rules.{h,cc}
├── tiktok_block_rules.{h,cc}
└── x_block_rules.{h,cc}
```

**To add a new platform:**
1. Create `<name>_block_rules.h` + `<name>_block_rules.cc` in `platform_rules/`
2. Add both to `platform_rules/BUILD.gn` sources
3. #include the header in `all_block_rules.cc` and call its `Get<Name>PrefixRules()` / `Get<Name>RegexRules()` in the two Append lambdas

### 2. Content Injection Rules — `chrome/browser/content_injection/platforms/`

Same pattern, per-platform files in `platforms/` with `content_injection_rules.cc` as the aggregator:

```
platforms/
├── BUILD.gn
├── facebook_rules.{h,cc}
├── instagram_rules.{h,cc}
├── linkedin_rules.{h,cc}
├── reddit_rules.{h,cc}
├── tumblr_rules.{h,cc}
├── x_rules.{h,cc}
└── youtube_rules.{h,cc}
```

**Content injection rules support two types** — `InjectionType::kCSS` (legacy, Base64 data-URI) and `InjectionType::kJavaScript` (preferred, injected as-is in an isolated world). CSS is auto-wrapped in a `<link>` element; JS payloads are assumed to be IIFEs and executed directly.

### JS snippet embedding pattern (v23+)

When the user provides a standalone JS snippet (e.g. from a `try_snippets/` directory), convert it into a C++ `constexpr std::string_view` using a raw string literal:

```cpp
// instagram_rules.cc — route-aware feed/stories/explore blocker
constexpr std::string_view kInstagramJS = R"JS(
(function() {
  // ... full JS snippet verbatim, no escaping needed ...
  // SPA-aware: patches pushState/replaceState + popstate listener
  ['pushState','replaceState'].forEach(m => {
    const orig = history[m];
    history[m] = function(...args) {
      const r = orig.apply(this, args);
      setTimeout(applyBlock, 0);
      return r;
    };
  });
  window.addEventListener('popstate', () => setTimeout(applyBlock, 0));
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"instagram.com", "", InjectionType::kJavaScript, kInstagramJS},
};
```

**Key points:**
- Use `R"JS(...)JS"` raw string delimiters — no escaping needed for quotes, backslashes, or newlines
- JS must be an IIFE to avoid polluting the isolated world across page visits
- SPA navigation handling lives INSIDE the JS (pushState/replaceState + popstate) — the C++ manager only injects once on DOMContentLoaded
- MutationObserver handles dynamically injected content from infinite scroll
- The aggregator (`content_injection_rules.cc`) just calls `Get<Name>InjectionRules()` — no per-platform logic there
- Per-platform files go in `platforms/<name>_rules.h` + `<name>_rules.cc`, registered in `platforms/BUILD.gn`

**Platform coverage (v23):**
| Platform | Type | Technique |
|----------|------|-----------|
| Instagram | JS | Route-aware feed/stories/explore blocker, video unloader, SPA-aware |
| YouTube | JS | Hides all content except header + pivot bar. Also covers youtu.be |
| Reddit | JS | Feed + faceplate loaders + scroll lock on `/`, SPA-aware |
| Facebook | JS | Posts + stories + skeletons + neutralizes IntersectionObserver triggers |
| X/Twitter | JS | Timeline, tablist, sidebar (trends/who to follow), Grok/Premium/Jobs |
| Pinterest | JS | Homefeed, masonry, closeup, related pins, footer, tablist. Nav/header preserved |
| LinkedIn | CSS | Legacy CSS (no JS snippet provided) |
| Tumblr | CSS | Legacy CSS (no JS snippet provided) |

```

## Apply fixes → build → push workflow

When review finds issues that need fixing, do everything in the patches repo first, then sync to checkout:

```bash
# 1. Fix the files IN the patches repo (not the checkout)
cd /home/hari/chromium-android/patches
# Edit files under patches/source-files/...

# 2. Sync fixed files to the Chromium checkout
cd /home/hari/chromium-android
python3 patches/apply-patches.py checkout/src patches/

# 3. Build (Docker ninja)
cd /home/hari/chromium-android
docker rm -f chromium-builder 2>/dev/null
docker compose run --name chromium-builder --rm \
  -e GCLIENT_SUPPRESS_GIT_VERSION_WARNING=1 \
  builder ninja -j3 -C out/Default chrome_public_apk

# 4. If build succeeds, commit + push patches
cd /home/hari/chromium-android/patches
git add -A && git commit -m "describe the fixes"
git push origin main
```

**Downstream repos may have commits local patches don't.** Before working, always `git pull origin main` in the patches dir so you have the latest architecture (e.g. per-platform rule files).

## Systematic patch review — when the user says "read the patches"

When debugging a Chromium fork and the user says "read the patches and find it," they mean review EVERY modified file — not just the one you suspect. Common failure mode: you find one bug, fix it, and declare victory while another crash lurks in a different file. 

**Do**: grep across ALL files under `source-files/` for the pattern class you're hunting (unguarded nulls, removed XML keys, stale method calls). Check signin files even if you're debugging Settings. Check NTP files even if you think the crash is elsewhere.

**Don't**: assume the first file you look at is the only one with issues. The code review earlier flagged a critical NPE in HistorySyncCoordinator that sat unfixed for multiple builds because nobody revisited it.

## Code Review Checklist (A+ quality gates)

See `references/code-review-findings.md` for detailed findings from the v17 quality pass.

Before marking a build ready:
1. **No `__pycache__/` in git** — add to `.gitignore` and remove
2. **No stray project files in root** — `Project_Progress.md` belongs in `docs/` or `notes/`
3. **No NPE risks** — any `mMediator = null` in constructor means `destroy()` MUST null-guard
4. **No dead code paths that waste observers** — if `canShowPromo()` always returns false, don't subscribe to observers
5. **Per-platform rules** — rules go in individual files, not the monolithic blocker
6. **Sync patches dir with checkout** — after any change, copy to `source-files/`
7. **Settings stripping: audit updatePreferences()** — when removing pref keys from XML, `addPreferenceIfAbsent()` calls in `updatePreferences()` MUST also be removed. `cachePreferences()` only caches XML keys; `assumeNonNull(mAllPreferences.get(removed_key))` crashes at runtime. Build won't catch this.

## Stable branch porting — API drift pitfalls

When moving patches from `main` to a stable release tag, APIs and files referenced by patches may not exist in the target branch. See `references/stable-branch-porting.md` for detailed patterns and fixes from the v149.0.7827.84 port.

**Quick pre-flight check after `apply-patches.py`:**

```bash
# 1. Check for resource index drift (XMLs on disk not in gni)
docker compose run --rm builder bash -c "
comm -23 <(find /checkout/src/chrome/android/java/res -name '*.xml' | sed 's|.*/chrome/android/||' | sort) \
         <(grep -oP '\"[^\"]+\.xml\"' /checkout/src/chrome/android/chrome_java_resources.gni | tr -d '\"' | sort)
"

# 2. Check for Java API drift (imports/calls to main-only classes)
docker compose run --rm builder bash -c "
grep -rn 'GlicHelper\|STANDBY_NO_FOCUS\|THEME_TIP\|shouldShowHomepageSettings\|setThemeTip\|NtpCustomizationPromoManager\|canShowComposeplateButtonOnNtp\|triggerCustomizationBottomSheet' /checkout/src/chrome/android/java/src/ 2>/dev/null
"

# 3. Check for C++ enum drift (mojom types that don't exist in stable)
docker compose run --rm builder bash -c "
grep -rn 'SUB_APPS\|MAIN_ONLY_FEATURE' /checkout/src/components/content_settings/ 2>/dev/null
"
```

Three categories of drift from this session (v149.0.7827.84):
1. **Resource index drift** — upstream XMLs not in gni → add missing entries (don't regenerate gni, it triggers full rebuild)
2. **Missing mojom enums** — `SUB_APPS_WITHOUT_PROMPTS` etc. → remove the Register block + verify closing brace
3. **Missing Java APIs** — NTP customization classes reworked between versions → remove/neutralize references

**⚠️ MANDATORY: Run pre-flight checks AFTER `apply-patches.py` and BEFORE `gn gen`/`ninja` on any version change.** Skipping this cost 3 build restarts at 90%+ progress on the v149 build — each restart was a 1-line fix that could have been caught in 30 seconds of grepping. Catching drift at step 1 saves 6-24h of failed build time.

**Do NOT skip this because "it's just a version bump."** Even minor release differences have API drift.

After the pre-flight, fix ALL issues found before starting the build. Do NOT fix one, start ninja, wait for the next error, fix another — batch them all.

**When sed edits keep breaking the same file:** Stop and download the clean stable original from GitHub. Multiple rounds of sed on a Java file create cascading brace/syntax errors that are faster to abandon than fix. Use comment-only SKIPs (no deletions, no structural changes) — see `references/stable-branch-porting.md` Category 5 and 6 for the full pattern with examples from v149.

## Stable branch workflow (v24+)

Chromium is now pinned to a stable release tag instead of tracking `main`. This reduces breakage from ~weekly API churn to ~monthly releases.

### Pinning to a specific version

The `Makefile` declares version variables:

```makefile
CHROMIUM_VERSION ?= 149.0.7827.84
CHROMIUM_TAG ?= refs/tags/$(CHROMIUM_VERSION)
```

`make fetch` runs `fetch --nohooks --no-history android` (required to initialize gclient) then `gclient sync --revision src@refs/tags/$(CHROMIUM_VERSION)` to pin.

### Switching versions

1. Update `CHROMIUM_VERSION` in `Makefile` and `patches/build-config/Makefile`
2. Nuke checkout → `make fetch` → `generate-patches.py` → `apply-patches.py`
3. Fix any API conflicts, build, push

**⚠️ `make fetch` can appear stalled but may still be working:** `gclient sync` and `git fetch` from chromium.googlesource.com can be extremely slow (~1 MiB/s with 1.2M+ objects). `git index-pack` may show 0% CPU for long periods — this is NOT a stall. `gclient` reports STALL DETECTED every 5 minutes as a false alarm during large fetches. **BEFORE killing**: (1) check `ps aux | grep index-pack` — if running, it's alive; (2) check `du -sh checkout/` twice 2 minutes apart — if growing, it's alive; (3) check `tail -20 build-logs/fetch.log` for HTTP 429 rate-limit errors vs just "Still working on" messages. Only kill if disk growth is flat for 15+ minutes AND no git process is running.

If truly rate-limited (HTTP 429), retry with reduced concurrency:

```bash
# Inside Docker, fetch the tag directly and pin it:
docker compose run --rm builder bash -c "
  cd /checkout/src
  git fetch origin refs/tags/149.0.7827.84 --depth=1
  git checkout FETCH_HEAD
  git tag -f 149.0.7827.84 HEAD
"
# Then sync deps with reduced concurrency:
docker compose run --rm builder bash -c "
  cd /checkout/src
  gclient sync --nohooks --jobs 1
"
```

**Why `--depth=1` + `FETCH_HEAD`:** A shallow fetch doesn't create a local tag ref. The commit lands in `.git/FETCH_HEAD`. Checkout `FETCH_HEAD` then tag it manually. If the current HEAD already matches the tag (common when main ≈ stable), skip the fetch entirely — just tag HEAD and sync deps.

**Docker permission trap:** `generate-patches.py` must run INSIDE Docker (as root) or it fails with `PermissionError` on root-owned checkout files. Always mount the patches dir:

```bash
docker compose run --rm -v $(pwd)/patches:/patches builder \
  python3 /patches/generate-patches.py /checkout/src /patches
```

### generate-patches.py / apply-patches.py (dual-mode)

The patches repo now supports two modes:

**Patch Mode** (preferred): When `patches/*.patch` files exist:
- `generate-patches.py` creates `.patch` files by diffing modified `source-files/` against a clean checkout
- `apply-patches.py` copies only **new** files verbatim + applies `.patch` files via `git apply`
- Category B (surgical edits to throttles/BUILD.gn) are also diffed and stored as patches

**Legacy Mode** (fallback): When no `.patch` files exist, falls back to copying all files verbatim + string-replacement surgical edits.

### Build infrastructure (`build-config/`)

All build environment files are versioned in the patches repo under `build-config/`:

| File | Purpose |
|------|---------|
| `Dockerfile` | Ubuntu 22.04 build container |
| `docker-compose.yml` | Mounts and container config |
| `Makefile` | Full workflow: fetch → patches → deps → configure → build |
| `args.gn` | GN args (arm64, release, no symbols) |
| `.gclient` | Chromium source repo config |

Copy these to `~/chromium-android/` to recreate the build environment on any machine.

## Nuking a checkout with Docker root-owned files

Docker bind mounts create root-owned files in `checkout/`. `rm -rf` from the host fails with "Permission denied" (and `chmod -R u+w` also fails on root-owned files). `sudo` may not be available or may require an interactive password (no `sudo -S` with piped password). Fix: nuke from inside the container where you ARE root:

```bash
cd ~/chromium-android
docker compose run --rm builder bash -c "rm -rf /checkout/* /checkout/.* 2>/dev/null"
```

The host's `checkout/` directory is then empty and ready for a fresh fetch. The container's root user has full access to the bind-mounted volume.

## Docker disk space maintenance

Docker accumulates significant disk usage from old images and build cache. On a typical Chromium setup, Docker alone can consume **80+ GB**:

- Images (old builder versions, unused images): ~50 GB
- Build cache (intermediate layers): ~30 GB
- Stopped containers: ~5 GB

Periodically reclaim space:

```bash
docker system prune -a    # removes ALL unused images, containers, networks, build cache
                          # safe: won't touch the active builder image or running containers
```

After pruning, expect to reclaim 50-80 GB. The active `chromium-android-builder` image is preserved.

## Maintainability rule — fix the environment, not the source

When a build fails, prefer fixing the **build environment** (Dockerfile, toolchain, dependencies) over hacking Chromium source files. Source hacks (stubs, deleted plugins, modified configs) create dead touch-points that break on every stable update.

Examples of environment fixes (preferred):
- Adding Node.js to Dockerfile for napi compatibility
- Updating `args.gn` for missing build dependencies
- Installing missing system packages in Dockerfile

Examples of source hacks (avoid):
- Stubbing output files to skip DevTools bundling
- Deleting `node_modules` packages
- Modifying upstream rollup/GN build configs
- Creating placeholder drawables for dead resource entries (strip the entries instead)

## Node.js DevTools bundling failures (napi ABI mismatch)
## NTP Stripping — Silent ViewStub Hiding (v24+)

The preferred technique for stripping NTP UI elements (feed, tiles, composeplate, signin promo, home modules) without causing Java compilation errors. **Keep all ViewStubs in the XML — hide them, don't delete them.**

### The problem with deleting ViewStubs

When you delete a ViewStub from `new_tab_page_layout.xml`, the `R.id.*` constant disappears from the generated `R.java`. Any Java method that references that ID — even if the method is never called at runtime — fails compilation. The compiler resolves ALL symbols, including unreachable code paths.

### The technique: hide, don't delete

**Layer 1 — XML:** Keep every ViewStub. Add `android:visibility="gone"` and set dimensions to `0dp`:

```xml
<!-- Before: present and inflating -->
<ViewStub
    android:id="@+id/composeplate_view_stub"
    android:layout_width="match_parent"
    android:layout_height="@dimen/composeplate_view_height"
    android:layout="@layout/composeplate_view_layout" />

<!-- After: hidden — R.id preserved, never visible -->
<ViewStub
    android:id="@+id/composeplate_view_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/composeplate_view_layout"
    tools:visibility="gone" />
```

**Layer 2 — Java:** Comment out the *call sites* that trigger ViewStub `inflate()`. Do NOT touch the method bodies — just skip the calls:

```java
// SKIPPED: Composeplate removed from NTP (v24)
mIsComposeplateEnabled = false;  // prevents NPE from assumeNonNull() in height calc
// initializeComposeplateFlags(mProfile);
// if (assumeNonNull(mIsComposeplateEnabled)) {
//     initializeComposeplate();
// }

// SKIPPED: Home modules removed from NTP (v24)
// initializeHomeModules();

// SKIPPED: Most Visited Tiles removed from NTP (v24)
// initializeMostVisitedTilesCoordinator(
//         mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);

// SKIPPED: Sign-in promo removed from NTP (v24)
// if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
//     initializeSigninPromoCoordinator();
// }
```

**Layer 3 — Runtime safety:** Any nullable field that was initialized by a SKIPPed call needs a fallback value. `mIsComposeplateEnabled = false` prevents NPE when `setSearchBoxHeightBoundsVerticalInset()` calls `assumeNonNull(mIsComposeplateEnabled)`.

**Why this works on every branch:**
- R.id constants exist → Java compiles
- `inflate()` never called → no feed/tiles/promo rendered
- Comment-outs preserve brace structure → no syntax errors
- Zero structural changes to Java → no branch API drift issues

**Files affected:** `new_tab_page_layout.xml`, `NewTabPageCoordinator.java`

See `references/silent-viewstub-hiding.md` for the full v24 implementation with exact before/after diffs.

### Stale package-lock.json — missing platform-specific optional deps

Even with the right Node.js version, the DevTools `package-lock.json` may be missing platform-specific optional dependencies (like `@rollup/rollup-linux-x64-gnu`). This happens when the lockfile was generated on a different platform or with `--ignore-optional`. The error looks like:

```
Error: Cannot find module '@rollup/rollup-linux-x64-gnu'
```

**Fix — regenerate the lockfile inside the container:**

```bash
docker compose run --rm builder bash -c "
  cd /checkout/src/third_party/devtools-frontend/src
  rm package-lock.json
  npm install --no-audit --no-fund
"
```

This generates a fresh lockfile with all platform-appropriate optional deps (linux-x64-gnu, etc.). Do NOT just `npm install` the missing package — other optional deps may also be missing. A full regeneration catches everything.

Verify the native binding is now in the lockfile:
```bash
docker compose run --rm builder python3 -c "
import json; lock=json.load(open('/checkout/src/third_party/devtools-frontend/src/package-lock.json'))
print([n for n in lock.get('packages',{}) if 'rollup-linux' in n])
"
## Node.js DevTools bundling failures (napi ABI mismatch)

See `references/devtools-node-napi-error.md` for full error transcript and analysis.
The DevTools frontend uses rollup with a Rust-based native plugin (`@web/rollup-plugin-import-meta-assets`) that may fail with:

```
Error: Failed to convert napi value into rust type `bool`
```

This is an ABI mismatch between the prebuilt Node binary (`third_party/node/linux/node-linux-x64/bin/node`) and the Rust napi plugin — the bundled Node v24.12.0 was compiled against a newer glibc than Ubuntu 22.04's glibc 2.35.

**Do NOT stub output files or delete plugins.** These are Chromium build infrastructure components — hacking them makes the build unmaintainable and creates dead touch-points that break on every update.

**Proper fix — install Node.js 20 LTS in the Docker image:**

Add to `Dockerfile`:

```dockerfile
# Install Node.js 20 LTS (required for DevTools bundling)
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*
```

Then rebuild the image:

```bash
docker compose build
```

Node 20 LTS provides a napi-compatible runtime for the prebuilt rollup native modules. The system Node takes precedence over the bundled Chromium Node if `/usr/bin/node` is in `PATH` before `third_party/node/...`.

If the issue persists after installing Node 20, check `references/devtools-node-napi-error.md` for alternative approaches. DevTools bundling is not critical for Android APK builds, but prefer fixing the environment over stubbing outputs.

## `.git/` nuke — fast disk reclaim but breaks git operations

If the checkout's `.git/` balloons to 40-60 GB after a gclient sync (from temporary index-pack files), nuking it reclaims space immediately:

```bash
docker compose run --rm builder bash -c "rm -rf /checkout/src/.git && git init /checkout/src"
```

**⚠️ Consequence:** All git operations break — `git checkout`, `git diff`, `git show`. This means:
- `generate-patches.py` CANNOT run (it uses `git diff` and `git checkout`)
- `git apply` in `apply-patches.py` still works (it doesn't need history)
- `gn gen` and `ninja` still work (they don't need git)

Only nuke `.git/` as a last resort to free disk space mid-build. After nuking, you can't regenerate patches — fix files directly in the checkout and sync back to `source-files/` manually.

### Syncing checkout → source-files without git (root-owned files)

When `.git/` is gone, you can't use `git diff` or `git checkout`. And **all checkout files are owned by root** (Docker bind mounts), so `cp` from the host fails with "Permission denied". `sudo` may require an interactive password.

**Use Docker to bridge:** mount the source-files directory as a volume and copy from inside the container (where you ARE root):

```bash
# Copy a single file
docker compose run --rm -v /home/hari/chromium-android/patches/source-files:/sf builder bash -c \
  "cp /checkout/src/path/to/ChangedFile.java /sf/path/to/ChangedFile.java"

# Bulk-sync multiple files from checkout to source-files
docker compose run --rm -v /home/hari/chromium-android/patches/source-files:/sf builder bash -c "
  for f in \\
    chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPageCoordinator.java \\
    chrome/browser/ui/android/signin/.../SigninPromoMediator.java \\
  ; do
    mkdir -p /sf/\$(dirname \"\$f\")
    cp /checkout/src/\$f /sf/\$f
  done
"
```

After syncing, regenerate the `.patch` files manually (since `generate-patches.py` needs a clean checkout with `.git/`) or craft minimal patches by hand. Then commit from the patches repo.

**Same technique works for writing patch files** (also root-owned):
```bash
docker compose run --rm -v /tmp/my_patch.patch:/tmp/p.patch:ro \
  -v /home/hari/chromium-android/patches/patches:/patches builder bash -c \
  "cp /tmp/p.patch /patches/<filename>.patch"
```

## Monitoring long builds — Docker output buffering

Docker's output layer buffers stdout from inside the container. Even with `stdbuf -oL ninja ...` inside the container, the process manager's `output_preview` will be empty until the build completes. The build IS running — you just can't see live ninja output.

**Monitor via three methods instead:**

```bash
# 1. Check container is alive + ninja is running
docker top $(docker ps --filter "ancestor=chromium-android-builder" -q) | grep ninja

# 2. Check .ninja_log modification time (updates every step)
stat -c "%y" /home/hari/chromium-android/checkout/src/out/Default/.ninja_log

# 3. Check for errors in build output (only visible on completion)
# The process log will show everything when the build exits
```

**Polling interval:** During long builds, poll every 60 seconds. Check for:
- Container still alive? If exited, check exit code immediately
- Any new containers? Stale containers from killed builds must be cleaned
- `.ninja_log` still updating? If stale for 2+ minutes, build may be stuck

Do NOT rely on `notify_on_complete` alone — errors can emerge mid-build (Rust depfiles, rollup bundling, resource file mismatches). Active monitoring catches them early instead of discovering a failed build 6 hours later.

## SSH user preference — give commands, not workarounds

When the user is operating over SSH and a build step fails, do NOT attempt workarounds (removing features, stubbing deps). Give the exact command to run and let the user decide. The user can retry rate-limited commands themselves — workarounds make the build less maintainable.

## Rust sysroot depfiles missing — `rustc_wrapper.py` FileNotFoundError

After an interrupted build, the Rust sysroot (`out/Default/local_rustc_sysroot/`) may have `.rlib` files but missing `.d` depfiles. `rustc_wrapper.py` crashes because it expects a depfile for every `.rlib`:

```
FileNotFoundError: local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib/libcore_core.rlib.d
```

**Fix — create empty stub depfiles inside the container:**

```bash
docker compose run --rm builder bash -c "
cd /checkout/src/out/Default/local_rustc_sysroot/lib/rustlib/aarch64-linux-android/lib
find . -maxdepth 1 -name '*.rlib' -exec sh -c 'touch \"\${1}.d\"' _ {} \;
ls *.d | wc -l   # should be 20-30 files
"
```

Ninja will warn `premature end of file; recovering` but recovers gracefully. The empty depfiles mean "no dependencies to track" for already-compiled artifacts — this is fine since the `.rlib` files are up-to-date. A full Rust stdlib rebuild is NOT needed.

## Source-file cleanup after consolidation (v25+)

When upstream touchpoints are consolidated (e.g. replacing 14 inline lines with a single wrapper call), the stock-file copies in `source-files/` become redundant — they're identical to the patch files. Clean them up:

```bash
# After consolidation, delete source-file copies of MODIFIED stock files.
# KEEP only files that don't exist upstream (custom modules):
#   - chrome/browser/navigation_policy/*       (our custom blocker)
#   - chrome/browser/content_injection/*        (our custom injection)
#   - chrome/browser/ui/webui/distraction_blocked/* (our WebUI)
# DELETE everything else — they're covered by .patch files:
#   - All Java files under chrome/android/java/src/
#   - All layout/values/xml files under chrome/android/java/res/
#   - All signin coordinators under chrome/browser/ui/android/signin/
#   - components/content_settings/ and components/signin/ files
```

**After cleanup, the separation is clean:**
- `source-files/` = only NEW files that don't exist upstream
- `patches/*.patch` = all modifications to upstream files

This prevents drift between source-file copies and patches. If a source-file exists for a stock file, it shadows the patch and `generate-patches.py` produces conflicts.

## C++ consolidation pattern — wrapper functions for upstream touchpoints

When custom code adds multiple `#include` lines and inline registration blocks to an upstream file, wrap them in a single function exposed from your custom module. The upstream file then needs only **1 `#include` + 1 function call**.

**Before (high merge-conflict surface):**
```cpp
// chrome_content_browser_client_navigation_throttles.cc
+#include "chrome/browser/content_injection/content_injection_manager.h"
+#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
...
+    // Block short-form video feed URLs...
+    registry.AddThrottle(
+        ShortsReelsBlockerThrottle::CreateForNavigation(registry));
+    content::WebContents* web_contents = handle.GetWebContents();
+    if (web_contents) {
+      ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
+      content_injection::ContentInjectionManager::CreateForWebContents(
+          web_contents);
+    }
```

**After (minimal diff — 2 lines changed):**
```cpp
// chrome_content_browser_client_navigation_throttles.cc
+#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
...
+    distraction_blocker::RegisterThrottlesAndHelpers(registry, handle);
```

**Wrapper implementation** (in your custom module — e.g. `shorts_reels_blocker.cc`):
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

**Declaration** (in `shorts_reels_blocker.h`):
```cpp
namespace distraction_blocker {
void RegisterThrottlesAndHelpers(
    content::NavigationThrottleRegistry& registry,
    content::NavigationHandle& handle);
}  // namespace distraction_blocker
```

**Why this works:** The upstream file (`chrome_content_browser_client_navigation_throttles.cc`) changes every release as Google adds/removes throttles. A 2-line diff has near-zero merge-conflict probability. A 20-line inline block conflicts on almost every release.

**When to use this pattern:**
- Any upstream file that receives 3+ lines of inline additions
- Files known to churn between releases (throttle registration, browser client init, settings menu building)
- When multiple custom modules need to register in the same upstream hook (throttle + TabHelper + content injection)

**Don't over-apply:** A single `#include` line or a one-line flag change doesn't need a wrapper. The pattern pays off when you're injecting 5+ lines.

## Custom resources lesson — don't add to upstream resource indices

Adding custom font assets (`.ttf` files) or drawables to `chrome_java_resources.gni` creates 5+ lines of diff in a file that changes moderately between releases. The alternative — an `android_resources` target with a dep in `chrome/browser/BUILD.gn` — also adds build-system complexity with unclear resource merge chains.

**Preferred approach: use system defaults.** Android's system font (Roboto) is always available, always renders correctly, and requires zero touchpoints. Unless the custom font is a hard requirement, don't add it. The Geist font was removed in v25 because it wasn't needed — simpler NTP, fewer merge conflicts.


## Lessons from v24 stable branch build (149.0.7827.84)
These are the hard-won lessons from a 6-day, 15+ attempt build cycle porting main-branch patches to stable.

### 1. `sed` is the wrong tool for Java edits

`sed` cannot balance braces, detect orphaned methods, or understand Java syntax. Every `sed`-based edit to `NewTabPageCoordinator.java` cascaded into more errors — broken method bodies, unbalanced closing braces, syntax errors at lines far from the edit. After 10+ restart cycles, the file was unrecoverable.

**Rule:** If you need more than a single-line comment-out, use Python to parse and modify Java. The `references/stable-branch-porting.md` Category 7 documents the Python-based body-emptying approach. If that's too complex, revert to clean stable and accept the feature loss — it's faster than debugging cascading sed artifacts.

### 2. Three-strikes rule — when to abandon a file

If the same file fails compilation 3+ times from different errors, **stop patching it.** Revert to clean stable. The cost of surgical fixes on a moving target exceeds the value of the feature being ported.

This happened twice in v24:
- `NewTabPageCoordinator.java`: 10+ Java errors → abandoned, restored to clean stable
- `new_tab_page_layout.xml`: 4 R.id errors → abandoned, restored to clean stable
- `SigninPromoMediator.java`: 5 type errors after a 200-line patch → reverted, replaced with a 1-line change

**Pattern:** When you find yourself saying "just one more fix," you've already lost. Revert, rebuild, and port selectively later.

### 3. Single-purpose patches survive branch changes

| Patch | Lines changed | Outcome |
|-------|-------------|---------|
| `SigninPromoMediator` v1 | 200+ (API migration, constructor changes, return types) | 5 compile errors |
| `SigninPromoMediator` v2 | **1 line** (`canShowPromo()` → `return false`) | Zero errors |
| `NewTabPageCoordinator` | 180+ (NTP UI stripping, JS injection hooks) | 10+ cascading errors |
| `new_tab_page_layout.xml` | 50+ (stripped ViewStubs) | 4 R.id errors |

Every patch that changed more than a single behavioral line failed on the stable branch. The 1-line change went through immediately.

**Rule:** If a patch changes more lines than the feature actually requires, it's too invasive. Diff source-files against the stable original and strip every unnecessary change. Code cleanups, API migrations, and "while I'm here" refactors are the enemy of cross-branch portability.

### 4. NTP Java is the most fragile API surface

The New Tab Page Java classes (`NewTabPageCoordinator.java`, `NewTabPageLayout.java`) are the most heavily reworked between main and stable. Main-branch NTP patches have near-zero portability. API churn includes:
- Composeplate ("AI Mode") — different feature flags, different ViewStub IDs
- Feed modules — different coordinator APIs, different lifecycle hooks
- Signin promos — different delegate interfaces, different type signatures
- Customization bottom sheet — completely different trigger APIs

**Rule:** If NTP stripping is critical, implement it against the stable branch from scratch. Never port NTP patches from main. Budget a full day for NTP-only changes on a new stable version.

### 5. Pre-flight checks prevent multi-hour wasted builds

Every failed v24 build crashed at 85-98% completion. Each restart wasted 2-8 hours. The root cause was always detectable with 30 seconds of grepping:

```bash
# Before starting ninja, grep for known drift patterns:
docker compose run --rm builder bash -c "
grep -rn 'missing_import_or_api' /checkout/src/chrome/android/java/
"
```

Three restarts were caused by issues a pre-flight check would have caught. **Run pre-flight checks always** — the skill section "Stable branch porting — API drift pitfalls" has the full checklist.

### 6. The SigninPromoMediator case study: minimalism wins

The original patch was 222 lines. It removed `AccountManagerFacade`, changed `getVisibleAccount()` return type from `CoreAccountInfo` to `DisplayableProfileData`, added null-safe SyncService guards, and changed method signatures throughout. Only ONE behavior change was actually needed: never show the sign-in promo.

The fix: revert the entire file to the stable 149 original. Change exactly one method body:

```java
boolean canShowPromo() {
    return false;  // This is the entire patch
}
```

Everything else — the null guards, the type migration, the interface cleanup — was unnecessary noise that broke compilation. The `return false` covers every call site because every promoter checks `canShowPromo()` before rendering.

## Build pitfalls (recurring errors from Chromium changes)

### XML ViewStub stripping breaks Java R.id references

When you strip ViewStub elements from layout XMLs (composeplate, MV tiles, signin promo, home modules), their `R.id.*` constants disappear. ANY Java method that references those IDs fails compilation — even if the method is never called at runtime (its call site is SKIPPed). The compiler resolves ALL symbols, including unreachable code.

**Quick fix (shipping fast):** Revert the layout XML to the original stable file. All `R.id.*` references compile fine. Only SKIP the method call sites (comment-out initializations). The XML stays pristine, the Java skips the features — zero compile errors.

**Proper fix:** Strip the layout AND empty/remove the Java method bodies. See `references/stable-branch-porting.md` Category 7 for the Python-based body-emptying approach.

### Overly invasive patches — don't change method signatures

A patch that only needs one behavioral change (e.g. `canShowPromo()` → `false`) should NOT also change method return types, remove constructor parameters, or rewire dependency injection. These "bonus cleanups" break when interfaces don't match the target branch. See `references/stable-branch-porting.md` Category 8 for the full `SigninPromoMediator.java` case study.

**Rule:** If a patch changes more lines than the feature requires, it's too invasive. Diff your source-files against the stable original and audit for unnecessary API changes.

### ⚠️ `replace_all=true` on Java files destroys unrelated code

The `patch()` tool's `replace_all=true` mode matches EVERY occurrence of `old_string` in the file — including structurally identical blocks in completely different methods. In v24, `replace_all=true` on a composeplate block corrupted `getSearchBoxBoundsOnScreen()` and `getSearchBoxBoundsVerticalInset()` because those methods contained identical brace patterns.

**Never use `replace_all=true` on Java files.** Java has common structural patterns (if-else chains, try-catch blocks) that produce byte-identical code in unrelated methods. Use unique surrounding context (2-3 lines of distinct code above the target block) to ensure a single match, or use Python to make surgical changes.

### Wrong GN target for base64

The dependency `//base:base64` does NOT exist. The header `base/base64.h` is part of the main `//base` target. Correct:
```gn
deps = [
    "//base",  # includes base64.h — do NOT add "//base:base64"
]
```

### Nulled-in-constructor anti-pattern (HistorySyncCoordinator v22)

When you skip a sign-in/sync flow by calling `delegate.dismissXxx()` in the constructor and setting fields to null, EVERY other method that accesses those fields must null-guard. The constructor runs once; other methods can be called later by Chromium infrastructure.

```java
// DANGER — sets mMediator = null but declineAndDismiss() still calls mMediator.declineAndDismiss()
public HistorySyncCoordinator(...) {
    delegate.dismissHistorySync(false, false);
    mMediator = null;  // 💣
}
public void declineAndDismiss() {
    mMediator.declineAndDismiss();  // NPE!
}
// Also: setView(view, ...) accesses mMediator.getModel() unguarded
```

Fix: null-guard every access to nulled fields. Check `destroy()`, `setView()`, `declineAndDismiss()`, `maybeRecreateView()` — every public method.

### Stub overrides go stale when interfaces change

Anonymous stub implementations of Chromium interfaces (e.g. `FeedSurfaceScrollDelegate`, `NonNullObservableSupplier`) must implement **every** abstract method. When Chromium adds methods, stubs break. The error looks like:
```
error: <anonymous ...> is not abstract and does not override abstract method isChildVisibleAtPosition(int)
```

Fix: read the interface definition first, add the missing override. Common stub patterns:
```java
// FeedSurfaceScrollDelegate stub — check FeedSurfaceScrollDelegate.java for current set
return new FeedSurfaceScrollDelegate() {
    @Override public boolean isScrollViewInitialized() { return false; }
    @Override public boolean isChildVisibleAtPosition(int position) { return false; }
    @Override public int getVerticalScrollOffset() { return 0; }
    @Override public void snapScroll() {}
};

// FeedSurfaceProvider stub — check NonNullObservableSupplier for current set  
return new FeedSurfaceProvider() {
    // ... implement all abstract methods
};
```

Always grep the interface definition file to get the current list of abstract methods — never assume it matches what was there last week.

## Settings stripping: the addPreferenceIfAbsent trap (v20 → v21 production bug)

When you strip preference keys from `main_preferences.xml`, the Java code in two places references them:

1. **`createPreferences()`** — uses `findPreference(key)` which returns null → just null-guard
2. **`updatePreferences()`** — uses `addPreferenceIfAbsent(key)` which calls `mAllPreferences.get(key)` followed by `assumeNonNull()`

`cachePreferences()` only populates `mAllPreferences` from XML keys. Keys stripped from XML are **never cached**. When `updatePreferences()` calls `addPreferenceIfAbsent(stripped_key)`, it crashes:

```
assumeNonNull(mAllPreferences.get("settings_promo_card"))  // null → NPE 💥
assumeNonNull(mAllPreferences.get("sign_in"))               // null → NPE 💥
```

**The build will NOT catch this** — it compiles fine, crashes at Settings open.

**Fix (v21):** Remove the entire `addPreferenceIfAbsent` call AND its conditional block from `updatePreferences()`. The key doesn't exist — there is nothing to add, remove, or update. No null guard — just delete the code block.

Keys affected in this build: `PREF_SETTINGS_PROMO_CARD`, `PREF_SIGN_IN`, `PREF_GOOGLE_SERVICES`.

## The "dismiss immediately" pattern — audit ALL public methods

When you strip a sign-in surface by setting its mediator/state fields to `null` in the constructor and immediately dismissing the delegate, you create a fragile object that CANNOT service any public method after construction. The constructor fires `dismissXxx()`, the caller cleans up, and the object is GC'd — in theory. In practice:

- Async callbacks, lifecycle events, or view recreation can trigger public methods AFTER construction
- `destroy()` is the obvious one, but `declineAndDismiss()`, `maybeRecreateView()`, `setView()`, `reset()`, etc. are ALL at risk
- The compiler can't help — `@Nullable` annotations are suppressed by `assumeNonNull`

**Rule:** Before committing, grep the file for every reference to `mMediator` (or whichever field you nulled) and ensure EVERY one has a null guard. `assumeNonNull` is a guard; bare `mMediator.foo()` is a crash.

**v17→v22 example:** `HistorySyncCoordinator` had `destroy()` guarded in v17, but `declineAndDismiss()` and `setView()` slipped through for 5 builds.

## Platform awareness when patching build flags

Before modifying a build flag, verify it's relevant to Android. DICE support (`enable_dice_support`) is desktop-only — already false on Android. Patching it is a no-op.

## Other Changes

- Navigation throttle + content injection: see `chrome/browser/navigation_policy/` and `chrome/browser/content_injection/`
- Custom WebUI (`chrome://distraction-blocked`): see `chrome/browser/ui/webui/distraction_blocked/`
