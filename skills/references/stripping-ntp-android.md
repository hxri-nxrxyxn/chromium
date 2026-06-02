# Stripping the New Tab Page to Search Bar Only (Android)

## ⚠️ Branch portability warning

The NTP Java API surface (NewTabPageCoordinator, NewTabPage, NewTabPageLayout) differs significantly between Chromium branches. The stripping approach documented here was developed against a main-line checkout. When porting to a **stable release tag** (e.g., 149.0.7827.84), expect:

- Different enum values (AutocompleteState changed between versions)
- Classes that don't exist on stable (GlicHelper, NtpCustomizationPromoManager, THEME_TIP)
- Different method signatures (shouldShowHomepageSettings → renamed/removed)
- Different boolean field names (mIsComposeplateEnabled → mCanShowComposeplateButton)

**When porting to a new branch:** download the clean stable original of each patched file first (Category 5 in `references/stable-branch-porting.md`), then apply ONLY comment-based SKIPs (Category 6). Do NOT use line-range sed deletes or structural changes — the risk of brace/syntax cascades across branches is high. See the v149 build session for full examples.

If the NTP stripping patch causes more than 2 build restarts on a new branch, **revert to clean stable and ship without it.** The distraction-blocker, signin-disabling, and content injection features are higher priority — NTP stripping can be re-added later with a branch-specific approach.
Remove all NTP content except the search bar/omnibox — no feed, no tiles, no cards, no promos.

## Files to Modify

### 1. Layout XML: `chrome/android/java/res/layout/new_tab_page_layout.xml`
Remove all `ViewStub` elements after `search_box_stub`. Keep only logo + search bar.

**Before:** logo_stub → search_box_stub → composeplate_stub → mv_tiles_stub → signin_promo_stub → home_modules_stub → tab_switcher_stub → spacer

**After:** logo_stub → search_box_stub (end)

### 2. Java Layout: `NewTabPageLayout.java`
- Remove `initializeSiteSectionView()` call from `onFinishInflate()`
- Remove the entire `initializeSiteSectionView()` method
- Remove unused imports: `ViewGroup`, `ViewStub`, `Log`, `TAG` constant

### 3. Coordinator: `NewTabPageCoordinator.java`
In `initialize()`:
- `initializeMostVisitedTilesCoordinator(...)` → comment out
- `initializeComposeplateFlags(mProfile)` + `initializeComposeplate()` → comment out
- `initializeHomeModules()` → comment out
- `initializeSigninPromoCoordinator()` → comment out

### 4. NTP Page: `NewTabPage.java`
Replace the `FeedSurfaceCoordinator` with a minimal `FeedSurfaceProvider` stub that wraps the NTP layout in a plain `FrameLayout`:

```java
FrameLayout rootView = new FrameLayout(activity);
rootView.setLayoutParams(new FrameLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.WRAP_CONTENT));
rootView.addView(mNewTabPageLayout);
mFeedSurfaceProvider = new FeedSurfaceProvider() {
    @Override public void destroy() {}
    @Override public TouchEnabledDelegate getTouchEnabledDelegate() {
        return (enabled) -> {}; }  // setTouchEnabled(boolean) returns void
    @Override public FeedSurfaceScrollDelegate getScrollDelegate() { return null; }
    @Override public UiConfig getUiConfig() { return null; }
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

Required new imports: `FrameLayout`, `Callback`, `TouchEnabledDelegate`, `FeedSurfaceScrollDelegate`, `UiConfig`.

## ⚠️ CRITICAL PITFALL: Dead Java methods still resolve R.id references

Even when calls are commented out, the **method bodies still compile** and reference deleted ViewStub resource IDs. This causes BUILD FAILURE:

```
error: cannot find symbol
  symbol:   variable composeplate_view_stub
  location: class id
```

**Fix:** Stub out the body of EVERY method that references a deleted stub. Don't just comment out the call — gut the method:

```java
private void initializeComposeplate() {
    // SKIPPED: Composeplate removed from NTP
}
private void initializeMostVisitedTilesCoordinator(...) {
    // SKIPPED
}
private void initializeSigninPromoCoordinator() {
    // SKIPPED
}
private void initializeHomeModulesImpl() {
    // SKIPPED
}
```

Also skip the secondary composeplate initialization block:
```java
if (mCanShowComposeplateButton != null) {
    // SKIPPED: Composeplate removed from NTP
}
```

**Verify before building:**
```bash
grep -n "composeplate_view_stub\|mv_tiles_container\|signin_promo_view_container_stub\|home_modules_recycler_view_stub\|tab_switcher_module_container_stub" \
  chrome/browser/ntp/NewTabPageCoordinator.java
```
Zero matches before building.

## ⚠️ CRITICAL PITFALL: `ObservableSuppliers.createNonNull()` — don't implement `NonNullObservableSupplier` directly

When stubbing `getRestoringStateSupplier()`, use `ObservableSuppliers.createNonNull(...)` — do NOT try to implement `NonNullObservableSupplier` as an anonymous class:

```java
// ✅ CORRECT:
@Override public NonNullObservableSupplier<Integer> getRestoringStateSupplier() {
    return ObservableSuppliers.createNonNull(
            FeedSurfaceProvider.RestoringState.NO_STATE_TO_RESTORE);
}

// ❌ WRONG — will NOT compile:
@Override public NonNullObservableSupplier<Integer> getRestoringStateSupplier() {
    return new NonNullObservableSupplier<Integer>() {   // ERROR!
        // NonNullObservableSupplier has addObserver(Callback<Integer>, int)
        // which you cannot implement from an anonymous class
    };
}
```

**Build error if you get this wrong:**
```
error: <anonymous ...> is not abstract and does not override abstract method
       addObserver(Callback<Integer>,int) in NonNullObservableSupplier
error: method does not override or implement a method from a supertype
```

**Required import at top of file:**
```java
import org.chromium.base.supplier.ObservableSuppliers;
```

## ⚠️ CRITICAL PITFALL: `assumeNonNull()` NPE on skipped init

When `initializeComposeplateFlags()` is skipped, `mCanShowComposeplateButton` stays null. Downstream methods that call `assumeNonNull(mCanShowComposeplateButton)` **crash at runtime**:

```java
// In setSearchBoxHeightBoundsVerticalInset():
int searchBoxHeight = NtpCustomizationUtils.getSearchBoxHeight(
    resources, assumeNonNull(mCanShowComposeplateButton));  // NPE!
```

**Fix:** Set the field to a safe default where the call would have been:
```java
// SKIPPED: Composeplate removed from NTP
mCanShowComposeplateButton = false;  // prevents NPE in setSearchBoxHeightBoundsVerticalInset()
```

## Why replacing FeedSurfaceCoordinator is necessary

Simply stripping the NTP layout XML isn't enough. `FeedSurfaceCoordinator` creates a root `FrameLayout` containing:
1. The NTP header (NewTabPageLayout) 
2. A `RecyclerView` for feed articles

Even with empty feed, the RecyclerView shows error/loading UI elements with text like "can't refresh discover" — visible on the NTP.

The fix is to replace `FeedSurfaceCoordinator` entirely with a plain `FrameLayout` via an anonymous `FeedSurfaceProvider` implementation.

## Test impacts (benign — tests not run on device)

These tests will fail because they look for removed views:
- `NewTabPageTest.java` — `R.id.mv_tiles_container`, `R.id.composeplate_view`, `R.id.mv_tiles_layout`
- `ShowNtpAtStartupTest.java` — `R.id.home_modules_recycler_view`, `R.id.mv_tiles_container`, `R.id.tab_switcher_module_container`
- `FeedV2NewTabPageTest.java`, `MostVisitedTiles*Test.java`, `NewTabPageMemoryLeakTest.java`

All fine — these are unit tests for components that no longer exist.
