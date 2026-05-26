// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_POLICY_SHORTS_REELS_BLOCKER_H_
#define CHROME_BROWSER_NAVIGATION_POLICY_SHORTS_REELS_BLOCKER_H_

#include <string_view>

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// NavigationThrottle that blocks navigations (including server-side redirects)
// to short-form video feed URLs across YouTube, Instagram, Facebook,
// Reddit, X, and LinkedIn.
//
// Responsibility split:
//   - This throttle handles hard URL blocks before any network request is made.
//   - ShortsReelsBlockerTabHelper handles same-document SPA navigations that
//     the throttle never sees (pushState / replaceState).
//
// The TabHelper is attached at the call site
// (chrome_content_browser_client_navigation_throttles.cc), not here, so the
// two classes remain independently testable.
class ShortsReelsBlockerThrottle final : public content::NavigationThrottle {
 public:
  // Always returns a throttle; per-URL filtering is done inside Check*
  // methods so callers don't need to pre-inspect the URL.
  static std::unique_ptr<ShortsReelsBlockerThrottle> CreateForNavigation(
      content::NavigationThrottleRegistry& registry);

  ~ShortsReelsBlockerThrottle() override;

  ShortsReelsBlockerThrottle(const ShortsReelsBlockerThrottle&) = delete;
  ShortsReelsBlockerThrottle& operator=(const ShortsReelsBlockerThrottle&) =
      delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  // Exposed for reuse by ShortsReelsBlockerTabHelper.
  // Returns BLOCK_REQUEST for blocked URLs, PROCEED otherwise.
  [[nodiscard]] static ThrottleCheckResult CheckURL(const GURL& url);

  // Redirects |web_contents| to chrome://distraction-blocked.
  // Increments the global block counter beforehand so the WebUI shows the
  // right count on load.
  static void NavigateToBlockPage(content::WebContents* web_contents);

  // Returns the total number of blocks this session.
  static int GetBlockCount();

  // Returns true if |path| (caller must lower-case first) begins with
  // |prefix| at a path-component boundary — so "/shortsfilm" does NOT match
  // prefix "/shorts", but "/shorts" and "/shorts/abc" do.
  [[nodiscard]] static bool PathMatchesPrefix(std::string_view path,
                                              std::string_view prefix);

 private:
  explicit ShortsReelsBlockerThrottle(
      content::NavigationThrottleRegistry& registry);
};

// WebContentsObserver + UserData that intercepts same-document (SPA)
// navigations on monitored pages — i.e. pushState/replaceState transitions
// the throttle never sees.
//
// When a blocked URL is detected after commit, the helper navigates the tab
// back (or to about:blank if there is no history entry to return to).
class ShortsReelsBlockerTabHelper final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ShortsReelsBlockerTabHelper> {
 public:
  ~ShortsReelsBlockerTabHelper() override;

  ShortsReelsBlockerTabHelper(const ShortsReelsBlockerTabHelper&) = delete;
  ShortsReelsBlockerTabHelper& operator=(
      const ShortsReelsBlockerTabHelper&) = delete;

 private:
  explicit ShortsReelsBlockerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ShortsReelsBlockerTabHelper>;

  // content::WebContentsObserver:
  // WebContentsDestroyed() intentionally not overridden — base class handles
  // cleanup via UserData lifetime.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Navigates away from |url| if it matches a blocked pattern.
  void MaybeBlockURL(const GURL& url);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NAVIGATION_POLICY_SHORTS_REELS_BLOCKER_H_
