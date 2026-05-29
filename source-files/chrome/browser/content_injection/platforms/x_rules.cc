// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/x_rules.h"

namespace content_injection {

namespace {

// x.com — MutationObserver-based injection for the home and explore timelines.
//
// CSS-only injection is insufficient here because X is a React SPA that fully
// reconstructs the timeline subtree on every soft navigation. MutationObserver
// is used to continuously remove matching elements as they are re-inserted.
//
// All snippets are IIFEs to avoid polluting the global scope of the isolated
// world between page visits.

// x.com/home — continuously remove the algorithmic "For You" timeline.
// Selectors target:
//   - div[aria-label="Timeline: Your Home Timeline"]  — main timeline container
//   - div[aria-label="Home timeline"]                 — alternate label X has used
//   - nav[role="navigation"][aria-live="polite"]       — bottom navigation bar
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
// Selectors target:
//   - div[aria-label="Timeline: Explore"]              — explore feed container
//   - div[role="tablist"][data-testid="ScrollSnap-List"] — "For You / Trending"
//                                                         tab selector
//   - nav[role="navigation"][aria-live="polite"]        — bottom navigation bar
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

constexpr InjectionRule kRules[] = {
    {"x.com", "/home",    InjectionType::kJavaScript, kXHomeJS},
    {"x.com", "/explore", InjectionType::kJavaScript, kXExploreJS},
};

}  // namespace

base::span<const InjectionRule> GetXInjectionRules() {
  return kRules;
}

}  // namespace content_injection
