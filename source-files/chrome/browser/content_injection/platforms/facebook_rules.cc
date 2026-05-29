// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/facebook_rules.h"

namespace content_injection {

namespace {

// facebook.com — hide Stories tray and Reels shelves injected into the feed.
//
// Hard URL blocks (throttle layer):  /reels/, /reel/, /watch/
// CSS injection (this layer):        Stories tray + Reels shelf rows in feeds
//
// Two pagelet names are targeted because Facebook has used both historically:
//   data-pagelet="stories_tray"  — older name
//   data-pagelet="StoriesTray"   — newer camelCase name
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

constexpr InjectionRule kRules[] = {
    {"facebook.com", "", InjectionType::kCSS, kFacebookShelvesCSS},
};

}  // namespace

base::span<const InjectionRule> GetFacebookInjectionRules() {
  return kRules;
}

}  // namespace content_injection
