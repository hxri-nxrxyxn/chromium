# Navigation Throttle Pattern (Modern API)

## NavigationThrottleRegistry API

Modern Chromium uses `NavigationThrottleRegistry` to register throttles. The old pattern (`throttles.push_back(Create(...))`) was replaced with `registry.AddThrottle(...)`.

### Header

```cpp
chrome/browser/navigation_policy/my_blocker.h
```

Includes needed:
- `content/public/browser/navigation_throttle.h` — base class
- `url/gurl.h` — for GURL type (NOT implicitly included)

### Registration

In `chrome/browser/chrome_content_browser_client_navigation_throttles.cc`:

```cpp
#include "chrome/browser/navigation_policy/my_blocker.h"

// In CreateAndAddChromeThrottlesForNavigation():
registry.AddThrottle(
    MyBlockerThrottle::MaybeCreateThrottleFor(registry));
```

### Static factory pattern

Throttles use a static factory method (not directly constructing into a vector):

```cpp
class MyBlockerThrottle : public content::NavigationThrottle {
 public:
  explicit MyBlockerThrottle(content::NavigationThrottleRegistry& registry);
  
  static std::unique_ptr<MyBlockerThrottle> MaybeCreateThrottleFor(
      content::NavigationThrottleRegistry& registry);
};
```

### URL matching APIs

For blocking specific URL patterns:

- `url.DomainIs("youtube.com")` — matches if domain is exactly "youtube.com"
- `url.path()` — returns the URL path component
- `base::StartsWith(url.path(), "/shorts", base::CompareCase::INSENSITIVE_ASCII)` — case-insensitive path prefix match
- `BLOCK_REQUEST` / `PROCEED` — ThrottleCheckResult values
