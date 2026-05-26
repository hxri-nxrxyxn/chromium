# SPA pushState Interception for Chromium Throttles

## Problem

`NavigationThrottle` only fires on actual page navigations (link clicks, typed URLs, redirects). SPA sites like YouTube load content via `history.pushState()` + fetch API calls — the URL changes in the address bar but **no NavigationThrottle fires**; `WillStartRequest` and `WillRedirectRequest` are not called.

Example: clicking the "Shorts" tab on youtube.com calls `pushState("/shorts")`.

## Broken Approach: JS Injection via ExecuteJavaScript

**DO NOT use `RenderFrameHost::ExecuteJavaScript()` inside `WillProcessResponse()`.** It crashes the renderer because the JS V8 context / `window` / `history` objects aren't initialized yet at that point in the navigation lifecycle.

## Working Approach: WebContentsObserver + WebContentsUserData

### Design

A `WebContentsObserver` subclass intercepts **all committed** main-frame URL changes, including pushState/replaceState same-document navigations, via `DidFinishNavigation`. It is attached at the registration call site (in `chrome_content_browser_client_navigation_throttles.cc`), not inside the throttle, keeping the two classes independently testable.

### TabHelper → `NavigateToBlockPage` with `should_replace_current_entry = true`

Once the TabHelper detects a blocked URL, it must navigate away. The key insight: **SPA pushState navigations are already committed to the session history stack** by the time `DidFinishNavigation` fires. If we add `chrome://distraction-blocked` on top, pressing Back returns to the blocked URL, the TabHelper catches it again, and we get an **infinite loop**.

**Fix:** Use `should_replace_current_entry = true` on `LoadURLParams`. This replaces the committed blocked URL entry with the block page entry:

```cpp
// In ShortsReelsBlockerThrottle::NavigateToBlockPage():
void ShortsReelsBlockerThrottle::NavigateToBlockPage(
    content::WebContents* web_contents) {
  g_block_count.fetch_add(1, std::memory_order_relaxed);
  content::NavigationController::LoadURLParams params(
      GURL("chrome://distraction-blocked"));
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  // ⚠️ Replace the committed blocked URL — don't stack on top
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(params);
}
```

Without `should_replace_current_entry = true`, the history chain becomes:
```
[Page-A, youtube.com/shorts/xxx (via pushState), chrome://distraction-blocked]
                                                              ↑ Back goes here → blocked again → infinite loop
```

With it, the history is:
```
[Page-A, chrome://distraction-blocked]
         ↑ Back goes to Page-A ✓ (the pushState entry is gone)
```

### Header pattern

Multiply-inherit `content::WebContentsObserver` + `content::WebContentsUserData<YourTabHelper>`. Declare with `WEB_CONTENTS_USER_DATA_KEY_DECL()`:

```cpp
class YourTabHelper final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<YourTabHelper> {
 public:
  ~YourTabHelper() override;
  YourTabHelper(const YourTabHelper&) = delete;
  YourTabHelper& operator=(const YourTabHelper&) = delete;

 private:
  explicit YourTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<YourTabHelper>;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MaybeBlockURL(const GURL& url);
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
```

### Constructor (must initialize both bases)

```cpp
YourTabHelper::YourTabHelper(content::WebContents* wc)
    : content::WebContentsObserver(wc),
      content::WebContentsUserData<YourTabHelper>(*wc) {}
```

### DidFinishNavigation — catches pushState

```cpp
void YourTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  MaybeBlockURL(navigation_handle->GetURL());
}
```

### MaybeBlockURL — delegate to the throttle's CheckURL, then NavigateToBlockPage

```cpp
void YourTabHelper::MaybeBlockURL(const GURL& url) {
  if (ShortsReelsBlockerThrottle::CheckURL(url).action() !=
      content::NavigationThrottle::BLOCK_REQUEST) {
    return;
  }
  ShortsReelsBlockerThrottle::NavigateToBlockPage(web_contents());
}
```

### Registration (in `chrome_content_browser_client_navigation_throttles.cc`)

```cpp
// Inside CreateAndAddChromeThrottlesForNavigation():
registry.AddThrottle(
    ShortsReelsBlockerThrottle::CreateForNavigation(registry));

content::WebContents* web_contents = handle.GetWebContents();
if (web_contents) {
  ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
}
```

`CreateForWebContents()` is idempotent — safe to call every navigation. Returns existing instance if already attached.

## Required Includes

| Include | Purpose |
|---------|---------|
| `content/public/browser/navigation_controller.h` | `GetController().GoBack()` / `LoadURLWithParams` |
| `content/public/browser/web_contents_user_data.h` | CRTP base, included from the header |
| `content/public/browser/web_contents.h` | `WebContents*` parameter types |
| `content/public/browser/navigation_handle.h` | `DidFinishNavigation` parameter |
| `content/public/browser/web_contents_observer.h` | `WebContentsObserver` base |
| `ui/base/page_transition_types.h` | `PAGE_TRANSITION_AUTO_TOPLEVEL` |
| `base/memory/ptr_util.h` | `base::WrapUnique` for throttles |

## Compilation Pitfalls

| Issue | Symptom | Fix |
|-------|---------|-----|
| Missing `NavigationController` include | `error: member access into incomplete type` | `#include "content/public/browser/navigation_controller.h"` |
| Wrong `ThrottleCheckResult` comparison | `error: invalid operands to binary expression` | Use `.action()`: `result.action() == PROCEED` |
| `WebContentsUserData` not initialized in constructor | `error: constructor must explicitly initialize the base class` | Add `WebContentsUserData<YourTabHelper>(*wc)` to initializer list |
| TabHelper created inside throttle | `navigation_handle()` returns null | Create at call site |
| Missing `WEB_CONTENTS_USER_DATA_KEY_IMPL` | Linker error for key symbol | Add `WEB_CONTENTS_USER_DATA_KEY_IMPL(YourClass);` in `.cc` |
| Missing `base/memory/ptr_util.h` | Unknown type `WrapUnique` | `#include "base/memory/ptr_util.h"` |
| Missing `ui/base/page_transition_types.h` | `no member named 'PAGE_TRANSITION_AUTO_TOPLEVEL'` | `#include "ui/base/page_transition_types.h"` |
| **SPA loop** | Pressing Back from block page reloads it with incremented counter | Set `should_replace_current_entry = true` on `LoadURLParams` |
