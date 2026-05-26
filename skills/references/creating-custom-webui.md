# Creating a Custom WebUI Page

How to add a custom `chrome://your-page` to the Android Chromium fork. Used for the `chrome://distraction-blocked` block screen.

## Approach: Inline strings (no GRIT)

Two approaches exist for serving WebUI content:

## ⚠️ Critical: `base::RefCountedString` include

When serving inline HTML via `SetRequestFilter`, the `.cc` file **must** include:

```cpp
#include "base/memory/ref_counted_memory.h"
```

Without it, `base::RefCountedString` is not visible in namespace `base` and the build fails with:

```
error: no member named 'RefCountedString' in namespace 'base'
```

This was hit in a session where the include was omitted — it is NOT transitively pulled in by `web_ui_data_source.h`.
- **GRIT resources** — add `.grd` entries, generate resource IDs, use `webui::SetupWebUIDataSource()` with auto-generated resource map. Better for complex pages but requires modifying the build system.
- **Inline C++ strings** — embed HTML/CSS/JS as `constexpr` string literals in the controller `.cc` file, serve via `WebUIDataSource::SetRequestFilter()`. Simpler, self-contained, no GRIT changes needed. This is what we use.

## Steps

### 1. Add the host constant

In `chrome/common/webui_url_constants.h`:

```cpp
inline constexpr char kChromeUIDistractionBlockedHost[] =
    "distraction-blocked";
```

Alphabetically placed among existing hosts.

### 2. Create the controller files

**`chrome/browser/ui/webui/your_page/your_page_ui.h`:**

```cpp
#ifndef CHROME_BROWSER_UI_WEBUI_YOUR_PAGE_YOUR_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_YOUR_PAGE_YOUR_PAGE_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class YourPageUI;

class YourPageUIConfig
    : public content::DefaultWebUIConfig<YourPageUI> {
 public:
  YourPageUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIYourPageHost) {}
};

class YourPageUI : public content::WebUIController {
 public:
  explicit YourPageUI(content::WebUI* web_ui);
  ~YourPageUI() override;
  YourPageUI(const YourPageUI&) = delete;
  YourPageUI& operator=(const YourPageUI&) = delete;
};

#endif
```

**`chrome/browser/ui/webui/your_page/your_page_ui.cc`:**

```cpp
#include "chrome/browser/ui/webui/your_page/your_page_ui.h"

#include "base/memory/ref_counted_memory.h"  // REQUIRED for base::RefCountedString
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

constexpr char kYourPageHTML[] = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Your Page</title>
  <style>/* inline CSS — see references/interstitial-css-variables.md for native look */</style>
</head>
<body>
  <div id="content">Hello</div>
</body>
</html>
)HTML";

void CreateAndAddHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIYourPageHost);
  // Add dynamic strings from C++
  source->AddString("yourKey", "Hello from C++!");
  // Serve the inline HTML
  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        return path.empty() || path == "index.html";
      }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(kYourPageHTML)));
          }));
}

}  // namespace

YourPageUI::YourPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  CreateAndAddHTMLSource(Profile::FromWebUI(web_ui));
}

YourPageUI::~YourPageUI() = default;
```

### 3. Register the config

In `chrome/browser/ui/webui/chrome_web_ui_configs.cc`:

```cpp
// Add include (alphabetically):
#include "chrome/browser/ui/webui/your_page/your_page_ui.h"

// Add inside RegisterChromeWebUIConfigs():
map.AddWebUIConfig(std::make_unique<YourPageUIConfig>());
```

### 4. Add sources to BUILD.gn

In `chrome/browser/ui/BUILD.gn`, add to the sources list (alphabetically):

```gn
"webui/your_page/your_page_ui.cc",
"webui/your_page/your_page_ui.h",
```

### 5. Rebuild

```bash
cd ~/chromium-android && make build
```

## Accessing the page

Navigate to `chrome://your-page` in Chromium.

## Dynamic data from C++

For inline-string WebUIs with `SetRequestFilter` (our default), `loadTimeData.getString()` does NOT work — `strings.m.js` is never generated without `UseStringsJs()`. Instead, **embed data directly in the HTML** by building it in the request filter callback:

```cpp
// Build HTML with values embedded (don't use base::StringPrintf with %s + raw strings):
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"

std::string BuildPageHTML(int count) {
  std::string counter = base::StringPrintf("blocked %d times", count);
  return base::StrCat({
    R"HTML(<div class="error-code">)HTML",
    counter,
    R"HTML(</div>)HTML"
  });
}

// In SetRequestFilter callback:
source->SetRequestFilter(
    base::BindRepeating([](const std::string& path) {
      return path.empty() || path == "index.html";
    }),
    base::BindRepeating(
        [](const std::string& path,
           content::WebUIDataSource::GotDataCallback callback) {
          int count = SomeModule::GetCount();
          std::string html = BuildPageHTML(count);
          std::move(callback).Run(
              base::MakeRefCounted<base::RefCountedString>(
                  std::move(html)));
        }));
```

For GRIT-based WebUIs (with `UseStringsJs()`), `loadTimeData` does work:
```js
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
const value = loadTimeData.getString('key');
```

But our project uses the inline approach.

## Wiring live data from another module

When the WebUI page needs to display data from the blocker (e.g. block counter):

1. **Add a public static accessor** on the throttle class:
```cpp
// In shorts_reels_blocker.h (public section):
static int GetBlockCount();
```

2. **Implement with an atomic counter** in the `.cc`:
```cpp
namespace {
std::atomic<int> g_block_count{0};
}  // namespace

int ShortsReelsBlockerThrottle::GetBlockCount() {
  return g_block_count.load(std::memory_order_relaxed);
}
```

3. **⚠️ DO NOT use `source->AddString()` + `loadTimeData.getString()`** — that only works with `UseStringsJs()` + GRIT. For inline `SetRequestFilter` WebUIs, **build the HTML dynamically** with the value embedded:

```cpp
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/memory/ref_counted_memory.h"

std::string BuildPageHTML(int count) {
  std::string text = base::StringPrintf("blocked %d time%s today",
      count, count == 1 ? "" : "s");
  return base::StrCat({
    R"HTML(<!doctype html>...<div class="error-code">)HTML",
    text,
    R"HTML(</div>...)HTML"
  });
}

// In SetRequestFilter — read counter inside the callback:
source->SetRequestFilter(
    ...,
    base::BindRepeating(
        [](const std::string& path,
           content::WebUIDataSource::GotDataCallback callback) {
          int count = ShortsReelsBlockerThrottle::GetBlockCount();
          std::string html = BuildPageHTML(count);
          std::move(callback).Run(
              base::MakeRefCounted<base::RefCountedString>(
                  std::move(html)));
        }));
```

The counter is incremented by `NavigateToBlockPage()` before navigating to the WebUI, so the first thing the page sees is the updated count.

## VSync with patches system

1. Copy new `.h`/`.cc` files to `patches/source-files/chrome/browser/ui/webui/your_page/`
2. Update `patches/apply-patches.py` to handle registration
3. Update `patches/INTEGRATION.md` with registration steps
4. Commit and push to GitHub

## Reference implementation

See `chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h` and `.cc` in the checkout or `patches/source-files/chrome/browser/ui/webui/distraction_blocked/`.
