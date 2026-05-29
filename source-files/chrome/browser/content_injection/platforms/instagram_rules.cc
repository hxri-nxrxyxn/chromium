// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/instagram_rules.h"

namespace content_injection {

namespace {

// instagram.com/ — hide Stories tray only; the main feed is intentional.
//
// Three selectors in descending stability order:
//   1. data-pagelet="story_tray"  — most stable, used since early Instagram PWA
//   2. aria-label="Stories"       — ARIA fallback when pagelet attr is absent
//   3. section:has(> div > ul > li > div > canvas)
//                                 — structural fallback targeting the canvas-
//                                   rendered carousel that Stories uses
constexpr std::string_view kInstagramHomeCSS = R"CSS(
  [data-pagelet="story_tray"],
  [aria-label="Stories"],
  section:has(> div > ul > li > div > canvas) {
    display: none !important;
  }
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

constexpr InjectionRule kRules[] = {
    {"instagram.com", "/",       InjectionType::kCSS, kInstagramHomeCSS},
    {"instagram.com", "/explore", InjectionType::kCSS, kInstagramExploreCSS},
};

}  // namespace

base::span<const InjectionRule> GetInstagramInjectionRules() {
  return kRules;
}

}  // namespace content_injection
