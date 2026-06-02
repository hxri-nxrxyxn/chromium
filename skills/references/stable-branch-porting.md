# Stable Branch Porting — API Drift Pitfalls

When moving Chromium patches from `main` to a pinned stable release tag,
APIs and files that exist on main may not exist in the target branch.
This session's v149.0.7827.84 build hit 3 categories of drift.

## Category 1: Upstream resource index drift

XML files exist in the source tree but are NOT listed in `chrome_java_resources.gni`.
The build fails at `prepare_resources.py`:

```
Error: Found files not listed in the sources list of the BUILD.gn target:
../../chrome/android/java/res/layout/accessibility_annotator_first_run_bottom_sheet.xml
../../chrome/android/java/res/layout/custom_tabs_toolbar.xml
```

These are real upstream Chromium files (check: `head -3 <file>` shows copyright headers).
The gni file in the stable tag is simply out of sync with the source tree.

**Fix:** Add missing entries to `chrome_java_resources.gni` in alphabetical order:

```bash
sed -i '/"java\/res\/layout\/custom_tabs_toolbar_button.xml"/i\  "java\/res\/layout\/custom_tabs_toolbar.xml",' chrome/android/chrome_java_resources.gni
sed -i '/"java\/res\/layout\/app_history_filter.xml"/i\  "java\/res\/layout\/accessibility_annotator_first_run_bottom_sheet.xml",' chrome/android/chrome_java_resources.gni
```

**Why NOT regenerate the entire gni from filesystem:** re-running `gn gen` rewrites all ninja rules,
causing a near-full rebuild (58k targets). A simple gni edit lets ninja continue from where it stopped.

**Lesson:** A pre-flight resource audit before `gn gen` would catch this. Template check:

```bash
# Find XMLs in res/ not listed in gni
cd /checkout/src
comm -23 <(find chrome/android/java/res -name '*.xml' | sed 's|chrome/android/||' | sort) \
         <(grep -oP '"[^"]+\.xml"' chrome/android/chrome_java_resources.gni | tr -d '"' | sort)
```

## Category 2: Main-branch enums missing in stable mojom

Our `content_settings_registry.cc` patch registered `SUB_APPS_WITHOUT_PROMPTS`,
an enum added AFTER the stable 149 branch. The build error:

```
error: no member named 'SUB_APPS_WITHOUT_PROMPTS' in 'content_settings::mojom::ContentSettingsType'
```

**Check:** the enum doesn't exist anywhere in `components/content_settings/core/common/content_settings.mojom`
on stable. Don't try to add the enum to mojom — that requires mojom codegen regeneration and risks
further API cascades.

**Fix:** remove the entire `Register()` block for the missing enum. For this case it was lines 836-845:

```bash
sed -i '835,845d' components/content_settings/core/browser/content_settings_registry.cc
# ⚠️ Check that the closing '}' of the outer function wasn't deleted
# If it was, re-insert it before the next function definition
```

## Category 3: NTP Java APIs that don't exist on stable

NTP customization features were significantly reworked between 149 and main.
Our patches referenced these main-only APIs:

| API | File | Fix |
|-----|------|-----|
| `AutocompleteState.STANDBY_NO_FOCUS` | NewTabPage.java | Stable's AutocompleteState has NO static constants — remove the line |
| `GlicHelper.Caller.NEW_TAB_PAGE` | NewTabPage.java | GlicHelper doesn't exist on stable — remove the call + import |
| `NtpCustomizationCoordinator.BottomSheetType.THEME_TIP` | NewTabPageCoordinator.java | Enum doesn't exist — remove the entire bottom sheet block |
| `NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference()` | NewTabPageCoordinator.java | Method doesn't exist — remove the call |
| `HomepageManager.shouldShowHomepageSettings()` | MainSettings.java | Method doesn't exist — replace with `true` (always show) |

**Pattern:** After `apply-patches.py`, grep for potential API drift BEFORE building:

```bash
# Check Java patches reference APIs that exist in the target
cd /checkout/src
# For each patched .java file, check its imports resolve
# Manual review of git diff is faster than a full compile
git diff HEAD -- chrome/android/java/src/ | grep '^+' | grep -E '\.Caller|\.STANDBY|\.THEME_TIP|\.shouldShow'
```

## Category 4: Sloppy sed — check your deletions

Two sed-induced syntax errors in this session:

### Error A: Deleted the closing brace of a parent function

Removing lines 835-846 took the closing `}` of `ContentSettingsRegistry::Init()` with it, causing:
```
error: function definition is not allowed here
```
**Lesson:** When deleting a block inside a function, verify the parent's closing brace is preserved.

### Error B: Range-deleted method body but left the method signature orphaned

Deleting the THEME_TIP block from `triggerCustomizationBottomSheet()` with `sed '/THEME_TIP/,/TimeUtils/d'` removed the body but left `.create(` hanging:
```java
mNtpCustomizationCoordinator =
        NtpCustomizationCoordinatorFactory.getInstance()
                .create(
                        mActivity,
                        mBottomSheetController,
                        mTab::getProfile,
    }   // ← orphaned brace, ".create(" never closed
```
**Lesson:** Range deletes (`/START/,/END/d`) don't understand code structure. They delete EVERYTHING between the two patterns including lines you didn't intend. When removing a method body, delete the ENTIRE method (signature + body), not just the problematic lines. After ANY sed edit, read ±10 lines to verify structural integrity.

### Error C: One-at-a-time fix churn — audit ALL references when one fails

When `NtpCustomizationPromoManager` failed at step 15k, we fixed only that reference. The build restarted, then failed at `THEME_TIP`, restarted, then `shouldShowHomepageSettings`, restarted... Each fix was 1 line, but each restart wasted minutes. 

**Rule:** When the first main→stable API drift appears, grep ALL patched files for ALL patterns that might drift BEFORE restarting:
```bash
cd /checkout/src
grep -rn '\.Caller\b\|\.STANDBY\|\.THEME_TIP\|\.shouldShow\|setThemeTip\|canShowComposeplateButton' chrome/android/java/src/
```

Fix all hits at once, then restart once. Three restarts at 90%+ build progress is ~5 minutes per restart on the M710q for 1,800 remaining targets — that's 15 minutes wasted on three 1-line fixes.

**General fix pattern:** After any sed deletion, read ±10 lines around the edit to verify structure.
```bash
sed -n '825,850p' path/to/file.cc   # verify what's around the edit
```

## Category 5: When incremental fixes cascade — download clean stable original

When sed edits keep introducing new brace/syntax errors (the file gets worse each edit),
stop. Download the clean unmodified file from the stable tag on GitHub and start fresh:

```bash
# Inside Docker container:
curl -sL "https://raw.githubusercontent.com/chromium/chromium/refs/tags/149.0.7827.84/chrome/android/java/src/org/chromium/chrome/browser/ntp/NewTabPageCoordinator.java" \
  -o /tmp/clean_file.java
```

This happened with `NewTabPageCoordinator.java` on v149 — 4 rounds of sed created 5 different
brace/syntax errors. The clean file (1478 lines, 179/179 balanced braces) was downloaded,
then ONLY the compatible SKIP changes were applied — zero structural changes, zero deletions.

**When to use this:** after 2+ failed sed fix attempts on the same file during a stable port.
Each restart at 90%+ build progress wastes minutes; a clean base + minimal edits saves cycles.

## Category 6: Comment-out, don't delete — for structural changes

When disabling features on a stable branch, **comment out** initialization calls rather than
deleting them. Deletions remove braces and risk syntax errors; comments preserve structure.

**Good (no brace changes):**
```java
// SKIPPED: Most Visited Tiles removed from NTP
// initializeMostVisitedTilesCoordinator(
//         mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);
```

**Bad (brace deletion risk):**
```bash
sed -i '/initializeMostVisitedTilesCoordinator/,/tileGroupDelegate, touchEnabledDelegate);/d' file.java
```

The successful v149 NTP fix used ONLY comment-based SKIPs on the clean stable original:
- `// SKIPPED: Most Visited Tiles removed from NTP`
- `// SKIPPED: Composeplate removed` with `mIsComposeplateEnabled = false;`
- `// SKIPPED: Home modules removed from NTP`
- `// SKIPPED: Sign-in promo removed`

Result: 179 open / 179 close braces after edits — zero syntax errors, build passed Java compilation.

## Category 7: R.id resource drift — XML-stripped ViewStubs break method bodies

**⚠️ QUICK FIX (when you just need to ship):** Revert the XML to the original stable file. The Java `R.id.*` references compile fine if the ViewStubs still exist in the layout. Only the call sites need SKIPping (commenting out initializations). An XML with all original ViewStubs + Java with comment-SKIPped call sites = zero R.id compile errors. Use this when you're iterating fast on a port and the XML stripping isn't critical.

**Long-term fix (when you DO want stripped XMLs):** Follow the body-emptying approach below, or download clean original and use comment-only SKIPs (Category 6).

## Category 8: Overly invasive patches — don't change method signatures or dependencies

A patch that changes one thing (e.g. `canShowPromo()` → `false`) should NOT also change method return types, remove constructor parameters, or rewire dependency injection. These "bonus cleanups" break when interfaces don't match the target branch.

**Example — SigninPromoMediator.java (v149, this session):**

Our patch changed `getVisibleAccount()` to return `DisplayableProfileData` instead of `CoreAccountInfo`, removed the `AccountManagerFacade` dependency, and rewired all callers. This caused 5 compile errors because `SigninPromoDelegate` on stable 149 still expects `CoreAccountInfo` in `refreshPromoState()`, `onPrimaryButtonClicked()`, and `getConfigForPrimaryButtonClick()`.

The patch was 100+ lines but only ONE line was needed:
```java
boolean canShowPromo() {
    return false;  // ← the only change needed
}
```

**Fix:** Revert to the clean stable file, apply only the minimal change. The original imports, fields, and method signatures are correct for the branch — don't touch them.

**Rule of thumb:** If a patch changes more lines than the feature requires, it's too invasive. For "disable signin promo" the feature needs exactly one line changed. The other 99 lines of API migration created 5 cascading errors that took 3 build restarts to diagnose.

**Check before building on each branch:** Diff your `source-files/` against the original stable. For each file, count lines changed vs lines needed for the feature. If the ratio is > 3:1, audit for unnecessary API changes.

## Category 9: Check all patched files for cross-file coupling before restarting

When one Java file fails with "cannot find symbol" for a type/interface change, DON'T just fix that one file and restart. The SAME interface mismatch likely affects OTHER files too:

```bash
# If SigninPromoMediator fails because SigninPromoDelegate expects CoreAccountInfo:
docker compose run --rm builder bash -c "
  grep -rn 'SigninPromoDelegate' /checkout/src/chrome/browser/ui/android/signin/
  grep -rn 'CoreAccountInfo' /checkout/src/chrome/browser/ui/android/signin/java/src/org/chromium/chrome/browser/ui/signin/signin_promo/SigninPromoDelegate.java
"
```

Our SigninPromoMediator fix was correct BUT we wasted a build restart because we didn't also check whether the delegate interface was on the same API — turns out it was fine (the delegate was unchanged), but the principle saves cycles.

## Category 7 (continued): R.id resource drift — XML-stripped ViewStubs break method bodies

When layout XMLs are stripped of ViewStub elements (composeplate, MV tiles, signin promo, home modules),
the corresponding `R.id.*` constants disappear. But the Java methods that inflate those ViewStubs are
still compiled — even if their call sites are SKIPPED. The compiler resolves all symbols, including
unreachable method bodies.

```java
// This method is NEVER called (call site is SKIPPED), but it still compiles
private void initializeComposeplate() {
    ViewStub stub = mNewTabPageLayout.findViewById(R.id.composeplate_view_stub);
    //                        error: cannot find symbol ^^^^^^^^^^^^^^^^^^^^^^^^
}
```

**Fix — empty the method bodies (Python approach):** Use Python inside the container to find
method boundaries and replace entire bodies with `{}`. This is safer than sed because Python
can count braces and find matching closes:

```python
targets = ["initializeComposeplate", "initializeSigninPromoCoordinator", "initializeHomeModulesImpl"]
# For each method: find "{", count depth, replace i..j-1 with "{ }"
```

**Pitfall — multi-line signatures:** Python must handle method signatures where `{` is on a
different line from the method name (e.g. `initializeMostVisitedTilesCoordinator` spans 4 lines).
Check for `{` on the same line OR in the lines immediately following the method name.

**Pitfall — double-brace output:** If the Python replacement produces `{ {}` instead of `{}`,
it left the original opening brace in place. Fix with: `sed -i "s/{ {}/{}/"` — this merges the
doubled opening brace into a clean empty body.

**Alternative (simpler):** Download the clean stable original from GitHub and use comment-only
SKIPs (Category 6). The method bodies are harmless if they compile — the original stable has
all R.ids. Only the call sites need SKIPping.

**When does this happen:** When your patches remove ViewStub XML elements (nuking composeplate,
signin promo, MV tiles, home modules) but leave the Java code intact. The build succeeds through
Java compilation only if R.ids exist in layout XMLs — removing them creates compile errors.

**Check before building:**
```bash
# Find Java methods referencing R.id.* that may have been stripped from XML
docker compose run --rm builder bash -c "
  grep -rn 'R.id.composeplate_view_stub\|R.id.mv_tiles_container\|R.id.signin_promo_view_container_stub\|R.id.home_modules_recycler_view_stub' /checkout/src/chrome/android/java/src/"
```
