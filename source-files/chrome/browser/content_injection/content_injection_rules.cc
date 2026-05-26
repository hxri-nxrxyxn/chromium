// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/content_injection_rules.h"

namespace content_injection {

namespace {

// ---------------------------------------------------------------------------
// CSS payloads
//
// CSS is preferred over JS wherever possible:
//   - No execution overhead or script parse cost.
//   - Applies before first paint — no flash of unwanted content.
//   - Page JS cannot interfere with injected styles in an isolated world.
//   - Survives React/Lit re-renders that swap element content but not
//     structure; for full SPA re-renders JS with MutationObserver is needed.
// ---------------------------------------------------------------------------

// instagram.com/ — hide Stories tray only; the main feed is intentional.
constexpr std::string_view kInstagramHomeCSS = R"CSS(
  [data-pagelet="story_tray"] { display: none !important; }
)CSS";

// instagram.com/explore — hide the algorithmic grid and its infinite-scroll
// machinery. Body scroll is locked so background loading stops entirely.
constexpr std::string_view kInstagramExploreCSS = R"CSS(
  /* Algorithmic explore grid */
  main div:has(> div > div > div > a[href*="/p/"]) {
    display: none !important;
  }
  /* Loading spinners / progress bars that would trigger further loads */
  [role="progressbar"],
  svg[aria-label="Loading..."],
  [data-visualcompletion="loading-state"] {
    display: none !important;
  }
  /* Lock scroll to kill the infinite-load loop entirely */
  body, html { overflow: hidden !important; }
)CSS";

// facebook.com — hide Stories tray and Reels shelves injected into the feed.
// Individual /reel/ and /reels/ URLs are hard-blocked at the throttle level.
constexpr std::string_view kFacebookShelvesCSS = R"CSS(
  /* Stories tray (both pagelet names Facebook has used) */
  div[data-pagelet="stories_tray"],
  div[data-pagelet="StoriesTray"] {
    display: none !important;
  }
  /* Reels shelf rows injected into the home feed */
  div[data-pagelet="FeedUnit"]:has(a[href*="/reels/"]) {
    display: none !important;
  }
)CSS";

// youtube.com — hide the Shorts shelf on the home page.
// Individual /shorts/ URLs are hard-blocked at the throttle level.
// Selector is keyed on the red Shorts icon SVG path fill value so it
// targets the section header precisely without relying on fragile class names.
constexpr std::string_view kYoutubeShortsShelfCSS = R"CSS(
  ytm-rich-section-renderer:has(span.yt-icon-shape svg path[fill="#f03"]) {
    display: none !important;
  }
)CSS";

// reddit.com — hide the algorithmic home feed. Subreddit pages (/r/…)
// are unaffected; this only matches the root feed path.
constexpr std::string_view kRedditHomeFeedCSS = R"CSS(
  shreddit-feed,
  #main-content {
    display: none !important;
    height: 0 !important;
    overflow: hidden !important;
  }
  faceplate-loader[name="HomeFeed_WnGPVB"] {
    display: none !important;
  }
)CSS";

// linkedin.com — hide the algorithmic feed on /feed/ and the home page.
// Profile pages (/in/…), search, and job listings are unaffected.
constexpr std::string_view kLinkedInFeedCSS = R"CSS(
  /* Main feed stream */
  .scaffold-finite-scroll__content,
  [data-finite-scroll-hotspot="true"] {
    display: none !important;
  }
  /* "Suggested posts" injected between followed content */
  .feed-follows-module,
  .feed-shared-update-v2 {
    display: none !important;
  }
)CSS";

// ---------------------------------------------------------------------------
// JavaScript payloads
//
// JS with MutationObserver is used only for heavy SPAs that reconstruct the
// entire feed subtree on every route change (X, Tumblr), where a one-shot
// CSS inject would be clobbered on the next render cycle.
//
// All snippets are IIFEs to avoid polluting the global scope of the
// isolated world between page visits.
// ---------------------------------------------------------------------------

// x.com/home — continuously remove the algorithmic timeline and nav bar.
constexpr std::string_view kXHomeJS = R"JS(
(function() {
  'use strict';
  const SELECTORS = [
    'div[aria-label="Timeline: Your Home Timeline"]',
    'div[aria-label="Home timeline"]',
    'nav[role="navigation"][aria-live="polite"]',
  ].join(',');

  const removeMatching = () => {
    document.querySelectorAll(SELECTORS).forEach(el => el.remove());
  };

  removeMatching();
  new MutationObserver(removeMatching).observe(document.body, {
    childList: true,
    subtree: true,
  });
})();
)JS";

// x.com/explore — remove Explore timeline, tab bar, and nav bar.
constexpr std::string_view kXExploreJS = R"JS(
(function() {
  'use strict';
  const SELECTORS = [
    'div[aria-label="Timeline: Explore"]',
    'div[role="tablist"][data-testid="ScrollSnap-List"]',
    'nav[role="navigation"][aria-live="polite"]',
  ].join(',');

  const removeMatching = () => {
    document.querySelectorAll(SELECTORS).forEach(el => el.remove());
  };

  removeMatching();
  new MutationObserver(removeMatching).observe(document.body, {
    childList: true,
    subtree: true,
  });
})();
)JS";

// tumblr.com/dashboard/stuff_for_you — remove algorithmic recommendations.
// The /dashboard/ main feed (/dashboard) is left untouched.
constexpr std::string_view kTumblrStuffForYouJS = R"JS(
(function() {
  'use strict';
  const SELECTORS = [
    'article',
    'div[aria-label="Notification"][role="dialog"]',
    '[aria-label*="Explore"]',
    '[role="tablist"]',
  ].join(',');

  const removeMatching = () => {
    document.querySelectorAll(SELECTORS).forEach(el => el.remove());
  };

  removeMatching();
  new MutationObserver(removeMatching).observe(document.body, {
    childList: true,
    subtree: true,
  });
})();
)JS";

// ---------------------------------------------------------------------------
// Rule table
//
// Rules are evaluated in order; ALL matching rules for a given URL fire.
// A later rule does not cancel an earlier one.
//
// Columns: domain, path_prefix, type, payload
//   path_prefix = "" → matches every path on the domain.
//   path_prefix matching is prefix-only (not component-boundary enforced),
//   so "/feed" also matches "/feed/following", "/feed/", etc.
//
// To add a new platform: append a row (or rows) here only.
// ---------------------------------------------------------------------------
constexpr InjectionRule kRules[] = {
    // ── Instagram ────────────────────────────────────────────────────────────
    {"instagram.com", "/",        InjectionType::kCSS,        kInstagramHomeCSS},
    {"instagram.com", "/explore", InjectionType::kCSS,        kInstagramExploreCSS},

    // ── Facebook ─────────────────────────────────────────────────────────────
    {"facebook.com",  "",         InjectionType::kCSS,        kFacebookShelvesCSS},

    // ── YouTube ──────────────────────────────────────────────────────────────
    {"youtube.com",   "",         InjectionType::kCSS,        kYoutubeShortsShelfCSS},

    // ── Reddit ───────────────────────────────────────────────────────────────
    {"reddit.com",    "/",        InjectionType::kCSS,        kRedditHomeFeedCSS},

    // ── LinkedIn ─────────────────────────────────────────────────────────────
    {"linkedin.com",  "/",        InjectionType::kCSS,        kLinkedInFeedCSS},
    {"linkedin.com",  "/feed",    InjectionType::kCSS,        kLinkedInFeedCSS},

    // ── X / Twitter ──────────────────────────────────────────────────────────
    {"x.com",         "/home",    InjectionType::kJavaScript, kXHomeJS},
    {"x.com",         "/explore", InjectionType::kJavaScript, kXExploreJS},

    // ── Tumblr ───────────────────────────────────────────────────────────────
    {"tumblr.com",    "/dashboard/stuff_for_you",
                                  InjectionType::kJavaScript, kTumblrStuffForYouJS},
};

}  // namespace

base::span<const InjectionRule> GetInjectionRules() {
  return base::span<const InjectionRule>(kRules);
}

}  // namespace content_injection
