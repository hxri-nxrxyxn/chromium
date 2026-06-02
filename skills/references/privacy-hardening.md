# Privacy & Account Hardening (Android)

## Goal
Strip all Google sign-in surfaces and privacy-sensitive defaults from Chrome for Android. No sign-in prompts, no buttons, no notification prompts.

## Approach
Disable features/behaviors in code (Approach B) — not removing build flags — to minimize merge conflicts on upstream rebase.

## 1. Sign-In Surface Stripping (Java)

### Avatar Button (Identity Disc)
**File:** `chrome/android/java/src/org/chromium/chrome/browser/identity_disc/IdentityDiscController.java`
```java
// Make canShow always return false
```

### Toolbar Sign-In Button
**File:** `chrome/browser/ui/android/toolbar/java/.../SigninButtonCoordinator.java`
```java
// Make isShown() return false
```

### FRE Fullscreen Sign-In
**File:** `chrome/browser/ui/android/signin/java/.../fullscreen_signin/FullscreenSigninCoordinator.java`
```java
// Make both Continue and Cancel skip to next page
```

### History Sync Opt-In
**File:** `chrome/browser/ui/android/signin/java/.../history_sync/HistorySyncCoordinator.java`
```java
// Dismiss immediately on creation
```

### All Sign-In Promos (Settings, Bookmarks, NTP, Recent Tabs, Passwords)
**File:** `chrome/browser/ui/android/signin/java/.../signin_promo/SigninPromoMediator.java`
```java
// Make canShowPromo() return false
```

## 2. Settings Backdoors

### "Allow Chrome sign-in" toggle
**File:** `chrome/android/java/src/.../sync/settings/GoogleServicesSettings.java`
```java
private static boolean shouldShowAllowSignIn(Profile profile) {
    return false;  // Hide toggle entirely, not just for child accounts
}
```

This toggle controls `Pref.SIGNIN_ALLOWED`. Leaving it visible lets anyone re-enable sign-in with one tap. Must be hidden for full lockdown.

**Search index:** The dynamic settings search provider also uses `shouldShowAllowSignIn()` — hiding the toggle automatically removes it from search results too.

## 3. C++ Sign-In Hardening

### kSigninAllowed Pref Default
**File:** `components/signin/internal/identity_manager/primary_account_manager.cc`
```c++
registry->RegisterBooleanPref(prefs::kSigninAllowed, false);  // default false
```

This gates ALL sign-in mutations at the C++ level — identity manager returns errors instead of proceeding.

## 4. Notification Default-Deny

### Content Settings Registry
**File:** `components/content_settings/core/browser/content_settings_registry.cc`
```c++
Register(ContentSettingsType::NOTIFICATIONS, "notifications",
         CONTENT_SETTING_BLOCK,     // changed from CONTENT_SETTING_ASK
         /*valid_settings=*/
         {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK},  // ASK removed
```

This changes the **initial default** for the NOTIFICATIONS content setting. On first run:
1. `ContentSettingsRegistry::Init()` registers BLOCK as default
2. `DefaultProvider::ReadDefaultSettings()` reads BLOCK
3. All permission checks see BLOCK — no prompt is shown

**Removing ASK from valid_settings** prevents users/admins from setting any site back to ASK manually.

### Why this is clean
- One-line change, no ripple effects
- Applies to both secure and insecure origins
- Doesn't remove the content setting type — just makes it always block

## Files Changed (Summary)

```
chrome/android/java/.../IdentityDiscController.java
chrome/.../toolbar/signin_button/SigninButtonCoordinator.java
chrome/.../fullscreen_signin/FullscreenSigninCoordinator.java
chrome/.../history_sync/HistorySyncCoordinator.java
chrome/.../signin_promo/SigninPromoMediator.java
chrome/.../sync/settings/GoogleServicesSettings.java
components/signin/internal/identity_manager/primary_account_manager.cc
components/content_settings/core/browser/content_settings_registry.cc\n```\n\n## 5. Settings Menu Stripping — Google Services & Password Manager\n\nRemoving entire sections from the Settings screen requires touching both XML and Java.\n\n### Files to modify\n\n| File | What to do |\n|------|------------|\n| `chrome/android/java/res/xml/main_preferences.xml` | Remove Preference entries from XML |\n| `chrome/android/java/src/.../settings/MainSettings.java` | Null-guard all `findPreference()` calls for removed keys |\n\n### Step 1: Remove preferences from main_preferences.xml\n\nDelete these blocks from the XML:\n- `PreferenceCategory android:key=\"account_and_google_services_section\"` — empty category header\n- `SignInPreference android:key=\"sign_in\"` — the sign-in entry\n- `ChromeBasePreference android:key=\"google_services\"` — the Google services link\n- `ChromeBasePreference android:key=\"autofill_and_passwords\"` — the combined autofill entry\n- `PreferenceCategory android:key=\"autofill_section\"` — the autofill section header\n- `PasswordsPreference android:key=\"passwords\"` — the password manager entry\n- `ChromeBasePreference android:key=\"autofill_payment_methods\"` — payment methods\n- `ChromeBasePreference android:key=\"autofill_addresses\"` — addresses\n- `ChromeBasePreference android:key=\"autofill_options\"` — autofill options\n\n### Step 2: Null-guard MainSettings.java\n\n⚠️ **CRITICAL:** Every `findPreference(key)` for a removed key MUST be null-guarded. The phone WILL crash if accessed:\n\n```java\n// SignInPreference — wrap the entire initialization block\nSignInPreference signInPreference = findPreference(PREF_SIGN_IN);\nif (signInPreference != null) {\n    // ALL sign-in flow code — activityless launcher setup + initialize()\n    signInPreference.initialize(profile, cache, facade, supplier);\n}\n\n// Google Services — guard setViewId + setIcon\nChromeBasePreference gp = findPreference(PREF_GOOGLE_SERVICES);\nif (gp != null) gp.setViewId(...);\n\n// Passwords — guard setProfile + click listener\nPasswordsPreference pp = findPreference(PREF_PASSWORDS);\nif (pp != null) { pp.setProfile(...); pp.setOnPreferenceClickListener(...); }\n```\n\n### Step 3: Gut updateAutofillPreferences() methods\n\nAll three autofill methods become no-ops since their preferences are removed:\n\n```java\nprivate void updateAutofillPreferences() {\n    // Autofill & Passwords section removed from settings.\n}\nprivate void updateAutofillAndPasswords() {\n    // Autofill & Passwords section removed from settings.\n}\nprivate void updateAutofillPreferencesPreAutofillAndPasswords() {\n    // Autofill & Passwords section removed from settings.\n}\n```\n\nAlso remove: `setManagedPreferenceDelegateForPreference(PREF_PASSWORDS)`.

### Step 4: Remove addPreferenceIfAbsent calls from updatePreferences()

⚠️ **CRITICAL PITFALL:** Null-guarding `createPreferences()` is NOT enough. `updatePreferences()` calls `addPreferenceIfAbsent()` for stripped keys (PREF_SETTINGS_PROMO_CARD, PREF_SIGN_IN). `addPreferenceIfAbsent()` calls `assumeNonNull(mAllPreferences.get(key))` — but `cachePreferences()` never cached removed keys → NPE crash when Settings opens.

Remove BOTH blocks from `updatePreferences()`:

```java
// REMOVE these from updatePreferences():
// if (ChromeFeatureList.isEnabled(DEFAULT_BROWSER_PROMO_ANDROID2)) { addPreferenceIfAbsent(PREF_SETTINGS_PROMO_CARD); ... }
// if (shouldShowSignInPref(getProfile())) { addPreferenceIfAbsent(PREF_SIGN_IN); } else { removePreferenceIfPresent(PREF_SIGN_IN); }
```\n\n### Files Changed (Settings Stripping)\n\n```\nchrome/android/java/res/xml/main_preferences.xml\nchrome/android/java/src/.../settings/MainSettings.java\n```
