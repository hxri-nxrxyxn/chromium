# Silent ViewStub Hiding — Full v24 Implementation

Applied to stable 149.0.7827.84. This replaces the old approach of deleting
ViewStubs from XML (which broke R.id references) with a three-layer technique.

## Files changed

- `chrome/android/java/res/layout/new_tab_page_layout.xml`
- `chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPageCoordinator.java`

## Layer 1: XML — hide 5 ViewStubs

Each ViewStub gets `android:visibility="gone"`, `android:layout_width="0dp"`,
`android:layout_height="0dp"`, and `tools:visibility="gone"` (for layout
editor hints). Original attributes (android:id, inflatedId, layout references,
margins) are preserved exactly.

```xml
<!-- Composeplate / "AI Mode" — HIDDEN: distraction-free NTP -->
<ViewStub
    android:id="@+id/composeplate_view_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/composeplate_view_layout"
    tools:visibility="gone" />

<!-- Most Visited Tiles — HIDDEN -->
<ViewStub
    android:id="@+id/mv_tiles_layout_stub"
    android:inflatedId="@+id/mv_tiles_container"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/mv_tiles_layout"
    tools:visibility="gone" />

<!-- Sign-in promo — HIDDEN -->
<ViewStub
    android:id="@+id/signin_promo_view_container_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/signin_promo_view_new_tab_page"
    tools:visibility="gone" />

<!-- Home modules / Discover feed — HIDDEN -->
<ViewStub
    android:id="@+id/home_modules_recycler_view_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/home_modules_recycler_view_layout"
    tools:visibility="gone" />

<!-- Single tab card — HIDDEN -->
<ViewStub
    android:id="@+id/tab_switcher_module_container_stub"
    android:layout_width="0dp"
    android:layout_height="0dp"
    android:visibility="gone"
    android:layout="@layout/tab_switcher_module_container"
    tools:visibility="gone" />
```

## Layer 2: Java — comment out 5 call sites

All changes are comment-outs — zero structural changes, zero brace modifications.
The method bodies remain intact (they compile fine because R.ids still exist).

### Call site 1: Most Visited Tiles (lines 351-352)

```java
// Before:
        initializeMostVisitedTilesCoordinator(
                mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);

// After:
        // SKIPPED: Most Visited Tiles removed from NTP (v24)
        // initializeMostVisitedTilesCoordinator(
        //         mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);
```

### Call site 2: Composeplate (lines 361-365)

```java
// Before:
        initializeComposeplateFlags(mProfile);
        mNtpSearchBox.setIsFuseboxEligible(Boolean.TRUE.equals(mIsComposeplateEnabled));
        if (assumeNonNull(mIsComposeplateEnabled)) {
            initializeComposeplate();
        }

// After:
        // SKIPPED: Composeplate removed from NTP (v24)
        mIsComposeplateEnabled = false;
        // initializeComposeplateFlags(mProfile);
        // mNtpSearchBox.setIsFuseboxEligible(Boolean.TRUE.equals(mIsComposeplateEnabled));
        // if (assumeNonNull(mIsComposeplateEnabled)) {
        //     initializeComposeplate();
        // }
```

**Critical:** `mIsComposeplateEnabled = false` is NOT a comment. It prevents
`NullPointerException` when `setSearchBoxHeightBoundsVerticalInset()` calls
`assumeNonNull(mIsComposeplateEnabled)` — without this initialization, the field
is null because `initializeComposeplateFlags()` was never called.

### Call site 3: Home modules (line 367)

```java
// Before:
        initializeHomeModules();

// After:
        // SKIPPED: Home modules removed from NTP (v24)
        // initializeHomeModules();
```

### Call site 4: Sign-in promo (line 379-381)

```java
// Before:
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            initializeSigninPromoCoordinator();
        }

// After:
        // SKIPPED: Sign-in promo removed from NTP (v24)
        // if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
        //     initializeSigninPromoCoordinator();
        // }
```

### Call site 5: Search-provider-change composeplate re-init (lines 761-775)

This block in `onSearchProviderChanged()` re-initializes composeplate flags
when the search provider changes. If not SKIPPed, it would re-enable composeplate
at runtime.

```java
// Before:
                boolean previousIsComposeplateEnabled = mIsComposeplateEnabled;
                initializeComposeplateFlags(mProfile);
                if (!previousIsComposeplateEnabled
                        && mIsComposeplateEnabled
                        && mComposeplateCoordinator == null) {
                    // If the composeplate view is enabled while mComposeplateCoordinator hasn't
                    // been initialized yet, initialize it now.
                    initializeComposeplate();
                }

                if (previousIsComposeplateEnabled != mIsComposeplateEnabled) {
                    // When the flag value is changed, the height of search box might be changed.
                    setSearchBoxHeightBoundsVerticalInset();
                    // Updates the composeplate view's visibility.
                    updateActionButtonVisibility();
                }

// After:
                // SKIPPED: Composeplate flag refresh + lazy-init removed (v24)
                // boolean previousIsComposeplateEnabled = mIsComposeplateEnabled;
                // initializeComposeplateFlags(mProfile);
                // if (!previousIsComposeplateEnabled
                //         && mIsComposeplateEnabled
                //         && mComposeplateCoordinator == null) {
                //     initializeComposeplate();
                // }
                // if (previousIsComposeplateEnabled != mIsComposeplateEnabled) {
                //     setSearchBoxHeightBoundsVerticalInset();
                //     updateActionButtonVisibility();
                // }
```

## Why this approach succeeds where deletion failed

| Approach | R.id errors | Java compile | Runtime safety | Branch portability |
|----------|-----------|-------------|----------------|-------------------|
| Delete ViewStubs from XML | 4 errors | ✗ fails | N/A | Brittle |
| Delete ViewStubs + strip Java methods | 0 | ✗ fails (sed artifacts) | N/A | Worse |
| **Hide ViewStubs + comment out calls** | **0** | **✓** | **✓** | **Excellent** |

The key insight: ViewStubs are the contract between XML and Java. Breaking
the contract (deleting stubs) means the Java can't resolve R.ids. Honoring the
contract (keeping stubs, hiding them) means both sides compile and run cleanly.
