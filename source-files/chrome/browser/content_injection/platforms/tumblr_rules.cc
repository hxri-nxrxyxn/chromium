// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/tumblr_rules.h"

namespace content_injection {

namespace {

// tumblr.com/dashboard/stuff_for_you — remove algorithmic recommendations.
//
// The regular /dashboard feed is intentionally left untouched.
// MutationObserver is required because Tumblr's "Stuff for You" section
// dynamically inserts posts after the initial page render.
//
// Selectors:
//   article                               — individual post cards
//   div[aria-label="Notification"][role="dialog"]
//                                         — notification popover overlay
//   [aria-label*="Explore"]               — Explore sidebar / widgets
//   [role="tablist"]                      — "For You / Following" tab bar
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

constexpr InjectionRule kRules[] = {
    {"tumblr.com", "/dashboard/stuff_for_you",
     InjectionType::kJavaScript, kTumblrStuffForYouJS},
};

}  // namespace

base::span<const InjectionRule> GetTumblrInjectionRules() {
  return kRules;
}

}  // namespace content_injection
