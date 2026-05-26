# Error Transcripts from Real Chromium Build Session

## Error 1: BUILD.gn source registration missing (linker stage)

```
ld.lld: error: undefined symbol: ShortsReelsBlockerThrottle::MaybeCreateThrottleFor(
    content::NavigationThrottleRegistry&)
>>> referenced by chrome_content_browser_client_navigation_throttles.cc:299
>>>               obj/chrome/browser/core/chrome_content_browser_client_navigation_throttles.o:(
>>>               CreateAndAddChromeThrottlesForNavigation(content::NavigationThrottleRegistry&))
clang++: error: linker command failed with exit code 1
ninja: build stopped: subcommand failed.
```

**Cause:** The .cc file was copied into the tree but NOT added to `chrome/browser/BUILD.gn` sources. The registration code compiled (referencing the function), but the implementation file was never compiled/linked.

**Fix:** Add to `chrome/browser/BUILD.gn` as a source_set dep or inline source — never both (duplicate symbols).

## Error 2: Missing GURL include (compile stage)

```
../../chrome/browser/navigation_policy/shorts_reels_blocker.h:26:38: error: unknown type name 'GURL'
   26 |   ThrottleCheckResult CheckURL(const GURL& url) const;
```

**Cause:** The header declared a method taking `const GURL&` but never included `url/gurl.h`. GURL is NOT transitively available from `navigation_throttle.h`.

**Fix:** Add `#include "url/gurl.h"` to the header file.

## Error 3: android_static_analysis = "build_server" (Java stage at 96%+)

```
Exception: AUTONINJA_BUILD_ID is not set. android_static_analysis = build_server requires autoninja integration.
FAILED: obj/base/activity_state_java__errorprone.stamp
```

**Cause:** Using `ninja` directly (not `autoninja`) means `AUTONINJA_BUILD_ID` env var is never set. Errorprone requires it for build-server mode.

**Fix:** Set `android_static_analysis = "off"` in args.gn.

## Error 4: wrong android_static_analysis value (GN gen stage)

```
ERROR at //build/config/android/config.gni:147:3: Assertion failed.
  assert(android_static_analysis == "on" || android_static_analysis == "off" ||
```

**Cause:** Used `"none"` instead of `"off"`. Valid values are `"on"` or `"off"` only.

## Error 5: ThrottleCheckResult == ThrottleAction comparison

```
error: invalid operands to binary expression ('ThrottleCheckResult' and 'content::NavigationThrottle::ThrottleAction')
   38 |   if (result == PROCEED && url.DomainIs("youtube.com")) {
```

**Cause:** `ThrottleCheckResult` doesn't have `operator==` for `ThrottleAction`. The error is confusing because GURL's operator== is tried as a candidate too.

**Fix:** Use `result.action() == PROCEED` instead of `result == PROCEED`.

## Error 6: WebContentsUserData<T> base not initialized

```
error: constructor for 'ShortsReelsBlockerTabHelper' must explicitly initialize
  the base class 'content::WebContentsUserData<ShortsReelsBlockerTabHelper>'
  which does not have a default constructor
```

**Cause:** `WebContentsUserData<T>` is a CRTP template with a required constructor parameter. The derived class's constructor must explicitly initialize it.

**Fix:**
```cpp
ShortsReelsBlockerTabHelper(content::WebContents* wc)
    : content::WebContentsObserver(wc),
      content::WebContentsUserData<ShortsReelsBlockerTabHelper>(*wc) {}
```

## Error 7: RenderFrameHost::ExecuteJavaScript crash (runtime, not compile)

Silent renderer crash when using the JS injection approach in WillProcessResponse. The APK compiles fine but crashes as soon as the user navigates to the target domain. No build-time error.

**Cause:** `WillProcessResponse()` fires before the renderer's JavaScript environment (V8 context, DOM) is initialized. Calling `ExecuteJavaScript()` at this point causes a null dereference or use-after-free in the renderer process.

**Fix:** Use the `WebContentsObserver` + `WebContentsUserData<T>` pattern instead of JS injection.

## Error 8: `namespace content::RenderFrameHost;` invalid forward declaration

```
../../chrome/browser/content_injection/content_injection_manager.h:13:35: error: expected '{'
   13 | namespace content::RenderFrameHost;
```

**Cause:** C++17 nested-namespace syntax does NOT work for forward-declaring classes. It declares `RenderFrameHost` as a namespace, not a class.

**Fix:**
```cpp
// ❌ BROKEN
namespace content::RenderFrameHost;

// ✅ CORRECT
namespace content {
class RenderFrameHost;
}
```

## Error 9: CheckURL is private (TabHelper can't reuse it)

```
../../chrome/browser/navigation_policy/shorts_reels_blocker.cc:198:35: error: 'CheckURL' is a private member of 'ShortsReelsBlockerThrottle'
  198 |   if (ShortsReelsBlockerThrottle::CheckURL(url).action() !=
```

**Cause:** When the TabHelper calls `CheckURL(url)` statically to reuse the throttle's block logic, `CheckURL` must be in `public:`. A `private:` declaration causes this compile error.

**Fix:** Move `CheckURL` and `PathMatchesPrefix` to the `public:` section.

## Error 10: `isolated_world_ids.h` wrong include path

```
../../chrome/browser/content_injection/content_injection_manager.cc:14:10: fatal error: 'content/public/browser/isolated_world_ids.h' file not found
   14 | #include "content/public/browser/isolated_world_ids.h"
```

**Cause:** The `ISOLATED_WORLD_ID_GLOBAL` / `ISOLATED_WORLD_ID_CONTENT_END` constants live in `content/public/common/`, not `content/public/browser/`.

**Fix:**
```cpp
#include "content/public/common/isolated_world_ids.h"   // ✅
// #include "content/public/browser/isolated_world_ids.h"  // ❌
```

## Error 11: `ExecuteJavaScriptInIsolatedWorld` argument order wrong

```
error: no matching constructor for initialization of 'base::OnceCallback<void (base::Value)>'
    590 |       JavaScriptResultCallback callback,
```

**Cause:** The function signature is `(script, callback, world_id)`, but code passed `(script, world_id, callback)`. The compiler tried to construct a `OnceCallback` from the world_id integer, which produced a confusing error message about callback constructors.

**Fix:**
```cpp
// ✅ CORRECT
frame->ExecuteJavaScriptInIsolatedWorld(
    base::UTF8ToUTF16(script),
    /*callback=*/base::NullCallback(),       // 2nd: callback
    content::ISOLATED_WORLD_ID_CONTENT_END); // 3rd: world_id
```

## Error 12: C++ raw string `R"(...)` terminated early by `)"` in data URI

```
../../chrome/browser/navigation_policy/shorts_reels_blocker.cc:202:4: error: expected unqualified-id
  202 |   });
      |    ^
../../chrome/browser/navigation_policy/shorts_reels_blocker.cc:205:1: error: extraneous closing brace ('}')
```

**Cause:** The custom block page HTML contains SVG data URIs like `data:image/svg+xml,...%3E")`. Inside a C++ raw string literal `R"(...)`, the `)"` sequence forces the end of the string, leaving the rest of the HTML as unparseable C++ code. The compiler reports errors at the *end* of the function, not at the actual split point.

**Fix:** Use a custom raw string delimiter that does not appear in the content:
```cpp
// ❌ BROKEN — data URI's )" closes R"(...)
return R"(<style>.icon{background-image:url("data:image/svg+xml,...%3E")}</style>)";

// ✅ WORKS — )BLOCK" only matches )BLOCK" not )"
return R"BLOCK(<style>.icon{background-image:url("data:image/svg+xml,...%3E")}</style>)BLOCK";
```

**Detection:** Search the HTML for `)"` sequences — every SVG data URI with `%3E"` will have one. If found, use a unique delimiter like `R"BLOCK(`, `R"CSS(`, or `R"HTML(`.

## Error 13: `base::StringPrintf` + raw string literal triggers format spec checking

```
error: 'FormatSpecTemplate<void>' is unavailable: Format specified does not match the arguments passed.
```

**Cause:** `base::StringPrintf` has a compile-time format spec checker (via Abseil's `str_format`). Passing a raw string literal `R"HTML(...%s...)HTML"` makes the checker see `%s` but with no template arguments, producing this obscure error.

**Fix:** Use `base::StrCat` to join string pieces instead:
```cpp
#include "base/strings/strcat.h"

// ❌ BROKEN — compile-time format check trips on %s in raw string
return base::StringPrintf(R"HTML(<div class="error-code">%s</div>)HTML", val.c_str());

// ✅ WORKS — join at the seam
return base::StrCat({
    R"HTML(<div class="error-code">)HTML",
    val,
    R"HTML(</div>)HTML"
});
```
