# INTEGRATION PATCH
# =================
# This file shows the exact lines to add to existing Chromium source files.
# All new files are in their own directories and do not require changes
# to any existing file except the two noted below.
#
# ─────────────────────────────────────────────────────────────────────────────
# 1. chrome/browser/chrome_content_browser_client_navigation_throttles.cc
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to includes (near the other navigation throttle includes):
#
#   #include "chrome/browser/content_injection/content_injection_manager.h"
#   #include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
#
# ADD inside CreateAndAddChromeThrottlesForNavigation(), after the last
# existing registry.AddThrottle() call:
#
#   // Block short-form video feed URLs (YouTube Shorts, Instagram Reels,
#   // Facebook Reels/Watch, Reddit Reels, X Reels, LinkedIn Reels).
#   // The TabHelper is attached here so it persists for same-document SPA
#   // navigations that bypass the throttle entirely.
#   registry.AddThrottle(
#       ShortsReelsBlockerThrottle::CreateForNavigation(registry));
#
#   content::WebContents* web_contents =
#       navigation_handle->GetWebContents();
#   if (web_contents) {
#     ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);
#   }
#
# ─────────────────────────────────────────────────────────────────────────────
# 2. chrome/browser/chrome_browser_main_parts_android.cc
#    (or wherever per-tab helpers are attached for Android — commonly
#     TabAndroid::InitializeContentViewCore or a TabHelperRegistrar)
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to includes:
#
#   #include "chrome/browser/content_injection/content_injection_manager.h"
#
# ADD where other tab helpers are attached:
#
#   // Inject CSS/JS to suppress algorithmic feed UI on social platforms.
#   content_injection::ContentInjectionManager::CreateForWebContents(
#       web_contents);
#
# ─────────────────────────────────────────────────────────────────────────────
# 3. chrome/browser/ui/BUILD.gn
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to the sources list (alphabetically, near webui/crashes/):
#
#   "webui/distraction_blocked/distraction_blocked_ui.cc",
#   "webui/distraction_blocked/distraction_blocked_ui.h",
#
# ─────────────────────────────────────────────────────────────────────────────
# 4. chrome/browser/ui/webui/BUILD.gn
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to the "configs" source_set's deps list:
#
#   "//chrome/browser/ui/webui/distraction_blocked",
#
# ─────────────────────────────────────────────────────────────────────────────
# 5. chrome/browser/ui/webui/chrome_web_ui_configs.cc
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to includes (alphabetically):
#
#   #include "chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h"
#
# ADD inside RegisterChromeWebUIConfigs():
#
#   map.AddWebUIConfig(std::make_unique<DistractionBlockedUIConfig>());
# ─────────────────────────────────────────────────────────────────────────────
#
# ADD to deps of the main "browser" target (or whichever target builds the
# two files above):
#
#   "//chrome/browser/content_injection:content_injection",
#   "//chrome/browser/navigation_policy:shorts_reels_blocker",
