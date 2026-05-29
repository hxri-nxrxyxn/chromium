// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/linkedin_rules.h"

namespace content_injection {

namespace {

// linkedin.com — hide the algorithmic feed on /feed/ and the home page (/).
//
// Profile pages (/in/…), search, job listings, and company pages are
// intentionally unaffected. The hard block layer handles /videos/reels.
//
// Selectors:
//   .scaffold-finite-scroll__content  — main feed scroll container
//   [data-finite-scroll-hotspot]      — infinite scroll trigger element
//   .feed-follows-module              — "Suggested posts" injected between
//                                       followed content
//   .feed-shared-update-v2            — individual feed post units
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

constexpr InjectionRule kRules[] = {
    {"linkedin.com", "/",     InjectionType::kCSS, kLinkedInFeedCSS},
    {"linkedin.com", "/feed", InjectionType::kCSS, kLinkedInFeedCSS},
};

}  // namespace

base::span<const InjectionRule> GetLinkedInInjectionRules() {
  return kRules;
}

}  // namespace content_injection
