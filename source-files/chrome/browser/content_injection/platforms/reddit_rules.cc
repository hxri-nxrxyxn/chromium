// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/reddit_rules.h"

namespace content_injection {

namespace {

// reddit.com — hide the algorithmic home feed.
//
// Subreddit pages (/r/…) are intentionally unaffected; this only matches
// the root feed path ("/"). The hard block layer handles /reels/ and
// mobile share short-links (/r/<sub>/s/<id>).
//
// Two selectors cover the feed:
//   shreddit-feed        — the main Web Component feed container
//   #main-content        — fallback container that wraps shreddit-feed
//   faceplate-loader[name="HomeFeed_WnGPVB"]
//                        — the lazy-load trigger; hiding it also stops
//                          background fetch requests for more content
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

constexpr InjectionRule kRules[] = {
    {"reddit.com", "/", InjectionType::kCSS, kRedditHomeFeedCSS},
};

}  // namespace

base::span<const InjectionRule> GetRedditInjectionRules() {
  return kRules;
}

}  // namespace content_injection
