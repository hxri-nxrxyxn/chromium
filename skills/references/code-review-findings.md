# Code Review Findings — v22 (May 2026)

## Critical fixes applied (all resolved as of v22)

### 1. HistorySyncCoordinator NPE — triple unguarded access (v17→v22)
**File:** `chrome/browser/ui/android/signin/java/.../history_sync/HistorySyncCoordinator.java`

Constructor sets `mMediator = null` and immediately calls `delegate.dismissHistorySync(false, false)`. Three methods accessed `mMediator` unguarded:

| Method | v17 fix | v22 fix |
|--------|---------|---------|
| `destroy()` → `mMediator.destroy()` | ✅ null-guarded | — |
| `declineAndDismiss()` → `mMediator.declineAndDismiss()` | ❌ missed | ✅ null-guarded |
| `setView()` → `mMediator.getModel()` | ❌ missed | ✅ added `&& mMediator != null` |

**Pattern:** When using the "dismiss immediately" pattern (nulling fields in constructor), audit EVERY public method for unguarded access to those fields. `destroy()` was caught in v17; `declineAndDismiss()` and `setView()` slipped through for 5 builds.

### 2. addPreferenceIfAbsent trap (v20→v21 production crash)
**File:** `chrome/android/java/.../settings/MainSettings.java`

When preference keys are stripped from `main_preferences.xml`, `cachePreferences()` never stores them in `mAllPreferences`. But `updatePreferences()` called `addPreferenceIfAbsent(stripped_key)` which does `assumeNonNull(mAllPreferences.get(key))` — NPE at runtime. Build compiles fine, crashes on Settings open.

**Fix:** Remove the entire `addPreferenceIfAbsent` call and its conditional block from `updatePreferences()`. The key doesn't exist — nothing to add, remove, or update.

### 3. Repo hygiene: __pycache__ and Project_Progress.md (v22)
Both removed from git tracking. `__pycache__/` was already in `.gitignore`; `Project_Progress.md` deleted from git index.

## Minor issues (non-crashing, tracked for follow-up)

### 4. IdentityDiscController dead code (~60 lines)

**Fix:** Gut both methods to comment-only bodies. The profile-data setup in `setProfile()` was already correctly skipping registerObservers.

### 5. SigninPromoMediator wasteful observer registrations
Constructor registers 3 observers (IdentityManager, SyncService, ProfileDataCache) even though `canShowPromo()` always returns false. Observer callbacks fire but `updateVisibility()` → `canShowPromo()` returns false, so nothing happens.

**Fix:** Wrap observer registrations in `if (mShouldShowPromo) { ... }` — the `destroy()` calls to remove observers are idempotent in Chromium's ObserverList, so no matching guard needed there.

## Build fixes applied

### BUILD.gn: `//base:base64` does not exist
`content_injection/BUILD.gn` had `//base:base64` in deps. The `base64.h` header is in the main `//base` target, not a separate `//base:base64` target. Removed the bogus dep — `//base` was already listed.

### NewTabPage: missing `isChildVisibleAtPosition` override
Chromium added `boolean isChildVisibleAtPosition(int)` to `FeedSurfaceScrollDelegate` interface. The stub in `NewTabPage.java`'s `initializeFeedSurfaceProvider()` didn't override it. Added `return false` override.

## Architecture notes (preserved from upstream)

The per-platform refactoring is solid:
- Block rules: `navigation_policy/platform_rules/<name>_block_rules.{h,cc}` → `all_block_rules.cc` aggregator
- Injection rules: `content_injection/platforms/<name>_rules.{h,cc}` → `content_injection_rules.cc` aggregator
- Adding a new platform: 3 changes (new .cc/.h pair + BUILD.gn sources + aggregator include)
- CSS injection uses Base64 data-URI (`<link rel="stylesheet" href="data:text/css;base64,...">`) — safer than JSON escaping

## Settings Stripping Crashes (v18/v19)

### MainSettings.java NPE after removing XML entries
**Root cause:** Removing preference entries from `main_preferences.xml` makes `findPreference(key)` return null. Without null guards, any access — `signInPreference.initialize()`, `googleServicePreference.setViewId()`, `passwordsPreference.setProfile()` — throws NPE and crashes the Settings page instantly.

**Fix pattern:**
```java
SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
if (signInPreference != null) {
    signInPreference.initialize(profile, cache, facade, supplier);
}
```

**Secondary issue:** Empty `<PreferenceCategory>` headers with no children. Remove the category XML element entirely if all its children are gone.

### Font asset registration
Adding .ttf files to `res/font/` requires listing them in `chrome/android/chrome_java_resources.gni`. Without this, the build fails with "Found files not listed in the sources list". Find the existing `reader_mode_lexend.xml` entry and add font assets right after it.
