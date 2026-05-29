# Integration Guide

How to wire the distraction-free patches into an existing Chromium checkout.
Run `apply-patches.py` to perform steps 1–2 automatically. Steps 3–5 require
manual edits to build files that vary across Chromium versions.

---

## 1. Navigation Throttles Registration

**File:** `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`

### Add includes (near other navigation throttle includes)

```cpp
#include "chrome/browser/content_injection/content_injection_manager.h"
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
```

### Register inside `CreateAndAddChromeThrottlesForNavigation()`

Add after the last existing `registry.AddThrottle()` call:

```cpp
// Block short-form video feed URLs (YouTube Shorts, Instagram Reels,
// Facebook Reels/Watch, Reddit Reels, X Reels, LinkedIn Reels).
// The TabHelper is attached here so it persists for same-document SPA
// navigations that bypass the throttle entirely.
registry.AddThrottle(
    ShortsReelsBlockerThrottle::CreateForNavigation(registry));

content::WebContents* web_contents = handle.GetWebContents();
if (web_contents) {
  ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
  content_injection::ContentInjectionManager::CreateForWebContents(
      web_contents);
}
```

---

## 2. Browser BUILD.gn Dependencies

**File:** `chrome/browser/BUILD.gn`

Add to the `deps` list of the main `browser` source_set (alphabetically, near `navigation_predictor`):

```gn
"//chrome/browser/content_injection:content_injection",
"//chrome/browser/navigation_policy:shorts_reels_blocker",
```

---

## 3. Distraction-Blocked WebUI — Sources

**File:** `chrome/browser/ui/BUILD.gn`

Add to the `sources` list (alphabetically, near `webui/crashes/`):

```gn
"webui/distraction_blocked/distraction_blocked_ui.cc",
"webui/distraction_blocked/distraction_blocked_ui.h",
```

---

## 4. Distraction-Blocked WebUI — Config Dependency

**File:** `chrome/browser/ui/webui/BUILD.gn`

Add to the `"configs"` source_set's `deps` list:

```gn
"//chrome/browser/ui/webui/distraction_blocked",
```

---

## 5. Distraction-Blocked WebUI — Registration

**File:** `chrome/browser/ui/webui/chrome_web_ui_configs.cc`

### Add include (alphabetically)

```cpp
#include "chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h"
```

### Register inside `RegisterChromeWebUIConfigs()`

```cpp
map.AddWebUIConfig(std::make_unique<DistractionBlockedUIConfig>());
```

---

## Automated Application

Steps 1 and 2 are handled by `apply-patches.py`:

```sh
python3 apply-patches.py /path/to/chromium/src /path/to/this/repo
```

Run with `--help` for full usage information.
